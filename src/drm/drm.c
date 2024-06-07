#include <soilleirwl/display.h>
#include <soilleirwl/session.h>
#include <soilleirwl/logger.h>

#include <soilleirwl/interfaces/swl_output.h>

#include <stdint.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <sys/mman.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct swl_buffer {
	uint32_t handle, fb_id;
	uint32_t format, height, width, pitch;
	size_t size, offset;

	uint8_t *data;
} swl_buffer_t;

typedef struct swl_drm_output {
	swl_output_t common;

	drmModeCrtcPtr original_crtc;
	drmModeConnectorPtr connector;

	swl_buffer_t buffer[2];
	int front_buffer;

	int pending;
	int shutdown;

	struct wl_list link;
} swl_drm_output_t;

typedef struct swl_drm_backend {
	swl_display_backend_t common;
	int fd, dev;

	struct wl_list outputs;

	struct wl_display *display;
	struct wl_event_source *readable;

	struct wl_listener deactivate;
	struct wl_listener activate;
}swl_drm_backend_t;

drmModeCrtc *swl_drm_get_conn_crtc(int fd, drmModeConnector *conn, drmModeRes *res) {
	drmModeEncoder *encoder = NULL;
	drmModeCrtc *crtc = NULL;

	if(conn->encoder_id) { /*There is already an encoder setup just use that*/
		encoder = drmModeGetEncoder(fd, conn->encoder_id);
	}

	if(encoder || encoder->crtc_id) {
		crtc = drmModeGetCrtc(fd, encoder->crtc_id);
		drmModeFreeEncoder(encoder);
	}

	return crtc;
}

int swl_drm_create_fb(int fd, swl_buffer_t *bo, uint32_t width, uint32_t height) {
	int ret;

	if(!bo) {
		swl_error("Invalid input create fb\n");
		return -1;
	}

	bo->width = width;
	bo->height = height;

	ret = drmModeCreateDumbBuffer(fd, width, height, 32, 0, &bo->handle, &bo->pitch, &bo->size);
	if(ret) {
		swl_error("Unable to create drmModeCreateDumbBuffer\n");
		return -1;
	}

	ret = drmModeAddFB(fd, width, height, 24, 32, bo->pitch, bo->handle, &bo->fb_id);
	if(ret) {
		swl_error("Unable to add fb to drm card\n");
		goto error_destroy;
	}

	ret = drmModeMapDumbBuffer(fd, bo->handle, &bo->offset);
	if(ret) {
		swl_error("Unable to map dumb buffer\n");
		goto error_fb;
	}

	bo->data = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bo->offset);
	if(bo->data == MAP_FAILED) {
		swl_error("MMAP failed\n");
		goto error_fb;
	}

	memset(bo->data, 0xaa, bo->size);
	return 0;

	error_fb:
	drmModeRmFB(fd, bo->fb_id);
	error_destroy:
	drmModeDestroyDumbBuffer(fd, bo->handle);
	return ret;
}


int swl_drm_destroy_fb(int fd, swl_buffer_t *bo) {
	int ret;

	if(!bo) {
		swl_error("Invalid input create fb\n");
		return -1;
	}

	munmap(bo->data, bo->size);

	drmModeRmFB(fd, bo->fb_id);

	drmModeDestroyDumbBuffer(fd, bo->handle);

	return 0;
}


static void render_surface_texture(swl_output_t *output, swl_output_texture_t *texture,
		int32_t xoff, int32_t yoff) {
	swl_debug("%p %p\n", texture, output);
	swl_drm_output_t *drm_output = (swl_drm_output_t*)output;
	uint32_t *dst = (uint32_t *)drm_output->buffer[drm_output->front_buffer].data;
	uint32_t width = drm_output->buffer[drm_output->front_buffer].width;

	if(texture->data) {
		for(uint32_t y = 0; y < texture->height; y++) {
			for(uint32_t x = 0; x < texture->width; x++) {
				dst[(y + yoff) * width + x + xoff] = ((uint32_t*)texture->data)[y * texture->width + x];
			}
		}
	}
}

void swl_output_copy(swl_output_t *output, struct wl_shm_buffer *buffer, int32_t width, int32_t height, int32_t xoff, int32_t yoff) {
	swl_drm_output_t *drm_output = (swl_drm_output_t*)output;
	uint32_t *src = (uint32_t *)drm_output->buffer[drm_output->front_buffer].data;
	uint32_t src_width = drm_output->buffer[drm_output->front_buffer].width;
	uint32_t *dst = wl_shm_buffer_get_data(buffer);
	swl_debug("src and dest %p %p\n", src, dst);
	if(src) {
		for(uint32_t y = 0; y < height; y++) {
			for(uint32_t x = 0; x < width; x++) {
				dst[y * width + x] = src[(y + yoff) * width + x + xoff];
			}
		}
	}
}

void swl_output_init_common(int fd, drmModeConnector *connector, swl_output_t *output) {
	output->mode.refresh = connector->modes->vrefresh;
	output->mode.width = connector->modes->hdisplay;
	output->mode.flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.height = connector->modes->vdisplay;

	output->scale = 1;
	output->tranform = WL_OUTPUT_TRANSFORM_NORMAL;
	output->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->height = connector->mmHeight;
	output->width = connector->mmWidth;
	output->model = "DRM Monitor";
	output->make = "DRM Monitor";
	output->name = "SWL-Monitor";
	output->description = "Uhhh output";

	output->draw_texture = render_surface_texture; 
	output->copy = swl_output_copy;
	wl_signal_init(&output->frame);
	wl_signal_init(&output->destroy);
}

swl_drm_output_t *swl_drm_output_create(int fd, drmModeRes *res, drmModeConnector *conn) {
	swl_drm_output_t *output = calloc(1, sizeof(swl_drm_output_t));
	output->connector = conn;
	swl_output_init_common(fd, conn, &output->common);
	output->original_crtc = swl_drm_get_conn_crtc(fd, conn, res);
	swl_drm_create_fb(fd, &output->buffer[0], output->connector->modes->hdisplay, output->connector->modes->vdisplay);
	swl_drm_create_fb(fd, &output->buffer[1], output->connector->modes->hdisplay, output->connector->modes->vdisplay);
	
	return output;
}

void swl_drm_outputs_destroy(int fd, struct wl_list *list) {
	swl_drm_output_t *output, *tmp;
	wl_list_for_each_safe(output, tmp, list, link) {
		wl_signal_emit(&output->common.destroy, &output);

		swl_drm_destroy_fb(fd, &output->buffer[0]);
		swl_drm_destroy_fb(fd, &output->buffer[1]);

		drmModeFreeCrtc(output->original_crtc);
		drmModeFreeConnector(output->connector);
		wl_list_remove(&output->link);
		free(output);
	}
}

int drm_create_outputs(int fd, drmModeRes *res, struct wl_list *list) {
	uint32_t count;
	drmModeConnector *connector = NULL;
	swl_drm_output_t *output;

	for(count = 0; count < res->count_connectors; count++) {
		connector = drmModeGetConnector(fd, res->connectors[count]);
		if(!connector) {
			swl_warn("Failed to get connector %d\n", res->connectors[count]);
			continue;
		} else if(connector->connection == DRM_MODE_CONNECTED) {
			output = swl_drm_output_create(fd, res, connector);
			wl_list_insert(list, &output->link);
			continue;
		}
		drmModeFreeConnector(connector);
	}
	
	return 0;
}

static void swl_output_release(struct wl_client *client, struct wl_resource *resource) {

}

static struct wl_output_interface swl_output_impl = {
	.release = swl_output_release,
};

static void swl_output_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	swl_drm_output_t *output = data;

	resource = wl_resource_create(client, &wl_output_interface, SWL_OUTPUT_VERSION, id);
	wl_resource_set_implementation(resource, &swl_output_impl, data, NULL);

	wl_output_send_geometry(resource,
			output->common.x, output->common.y, 
			output->common.width, output->common.height, 
			output->common.subpixel, output->common.make, 
			output->common.model, output->common.tranform);
	wl_output_send_mode(resource, output->common.mode.flags, 
			output->common.mode.width, output->common.mode.height,
			output->common.mode.refresh);
	wl_output_send_scale(resource, output->common.scale);
	wl_output_send_name(resource, output->common.name);
	wl_output_send_description(resource, output->common.description);
	wl_output_send_done(resource);
}

static void modeset_page_flip_event(int fd, unsigned int frame,
				    unsigned int sec, unsigned int usec,
				    void *data) {
	swl_drm_output_t *output = data;

	output->pending = false;
	if (!output->shutdown) {
		memset(output->buffer[output->front_buffer].data, 0xaa, output->buffer[output->front_buffer].size);

		wl_signal_emit(&output->common.frame, output);

		drmModePageFlip(fd, output->original_crtc->crtc_id, 
				output->buffer[output->front_buffer].fb_id, 
				DRM_MODE_PAGE_FLIP_EVENT, data);
		output->front_buffer ^= 1;
		output->pending = true;
	}
}

int swl_drm_readable(int fd, uint32_t mask, void *data) {
	drmEventContext ev = { 0 };
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;

	drmHandleEvent(fd, &ev);
	
	return 0;
}

int swl_drm_backend_stop(swl_display_backend_t *display) {
	swl_drm_backend_t *drm = (swl_drm_backend_t*)display;
	swl_drm_output_t *output;
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;

	wl_list_for_each(output, &drm->outputs, link) {
		output->shutdown = true;
		while(output->pending) {
			drmHandleEvent(drm->fd, &ev);
		}
		drmModeSetCrtc(drm->fd, output->original_crtc->crtc_id, 
				output->original_crtc->buffer_id, 0, 0, &output->connector->connector_id, 
				1, &output->original_crtc->mode);
	}

	return 0;
}

int swl_drm_backend_start(swl_display_backend_t *display) {
	swl_drm_backend_t *drm = (swl_drm_backend_t*)display;
	swl_drm_output_t *output;
	swl_debug("%p\n", drm);
	wl_list_for_each(output, &drm->outputs, link) {
		swl_debug("%p\n", output);
		output->common.global = wl_global_create(drm->display, &wl_output_interface, 
				SWL_OUTPUT_VERSION, output, swl_output_bind);
		drmModeSetCrtc(drm->fd, output->original_crtc->crtc_id, output->buffer[0].fb_id, 
				0, 0, &output->connector->connector_id, 1, output->connector->modes);
		output->front_buffer ^= 1;
		drmModePageFlip(drm->fd, output->original_crtc->crtc_id, 
			output->buffer[output->front_buffer].fb_id, DRM_MODE_PAGE_FLIP_EVENT, output);
		output->pending = true;
		wl_signal_emit(&drm->common.new_output, output);
	}
	
	return 0;	
}

void swl_drm_deactivate(struct wl_listener *listener, void *data) {
	swl_drm_backend_t *drm = wl_container_of(listener, drm, deactivate);
	swl_drm_output_t *output;
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;


	wl_list_for_each(output, &drm->outputs, link) {
		output->shutdown = true;
	}
}


void swl_drm_activate(struct wl_listener *listener, void *data) {
	swl_drm_backend_t *drm = wl_container_of(listener, drm, activate);
	swl_drm_output_t *output;
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;

	wl_list_for_each(output, &drm->outputs, link) {
		output->shutdown = false;
		drmModeSetCrtc(drm->fd, output->original_crtc->crtc_id, output->buffer[0].fb_id, 
				0, 0, &output->connector->connector_id, 1, output->connector->modes);
		output->front_buffer ^= 1;
		drmModePageFlip(drm->fd, output->original_crtc->crtc_id, 
			output->buffer[output->front_buffer].fb_id, DRM_MODE_PAGE_FLIP_EVENT, output);
		output->pending = true;
	}
}


swl_display_backend_t *swl_drm_create_backend(struct wl_display *display, swl_session_backend_t *session) {
	const char *drm_device;
	swl_drm_backend_t *drm;
	drmModeResPtr res;
	struct wl_event_loop *loop = wl_display_get_event_loop(display);

	drm_device = "/dev/dri/card0";
	if(getenv("SWL_DRM_DEVICE")) {
		drm_device = getenv("SWL_DRM_DEVICE");
	}

	drm = calloc(1, sizeof(swl_drm_backend_t));
	drm->dev = session->open_dev(session, drm_device, &drm->fd);

	res = drmModeGetResources(drm->fd);

	wl_list_init(&drm->outputs);

	drm_create_outputs(drm->fd, res, &drm->outputs);
	drmModeFreeResources(res);
	drm->display = display;
	wl_signal_init(&drm->common.new_output);

	drm->activate.notify = swl_drm_activate;
	drm->deactivate.notify = swl_drm_deactivate;

	wl_signal_add(&session->activate, &drm->activate);
	wl_signal_add(&session->disable, &drm->deactivate);

	drm->readable = wl_event_loop_add_fd(loop, drm->fd, WL_EVENT_READABLE,
			swl_drm_readable, drm);

	return (swl_display_backend_t*)drm;
}

void swl_drm_backend_destroy(swl_display_backend_t *display, swl_session_backend_t *session) {
	swl_drm_backend_t *drm = (void *)display;
	
	wl_event_source_remove(drm->readable);

	swl_drm_outputs_destroy(drm->fd, &drm->outputs);
	session->close_dev(session, drm->dev);
	close(drm->fd);
	free(drm);
}
