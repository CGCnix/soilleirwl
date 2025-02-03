#include <soilleirwl/logger.h>
#include <soilleirwl/renderer.h>

#include <soilleirwl/allocator/gbm.h>
#include <soilleirwl/backend/display.h>
#include <soilleirwl/backend/session.h>
#include <soilleirwl/interfaces/swl_output.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct swl_drm_output {
	swl_output_t common;

	uint32_t fb_id[2];
	swl_gbm_buffer_t *cursor;
	swl_renderer_target_t *cursor_target;

	drmModeCrtcPtr original_crtc;
	drmModeConnectorPtr connector;

	int pending;
	int shutdown;

	struct wl_list link;
} swl_drm_output_t;

typedef struct swl_drm_backend {
	swl_display_backend_t common;
	int fd, dev;
	struct gbm_device *gbm;

	struct wl_list outputs;
	
	swl_renderer_t *renderer;
	
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

	if(encoder && encoder->crtc_id) {
		crtc = drmModeGetCrtc(fd, encoder->crtc_id);
		drmModeFreeEncoder(encoder);
	}

	if(crtc == NULL) {
		uint32_t crtcs = drmModeConnectorGetPossibleCrtcs(fd, conn);
		for(uint32_t i = 0; i < res->count_crtcs; i++) {
			if(crtcs & (1 << i)) { /*Compatible CRTC*/
				crtc = drmModeGetCrtc(fd, res->crtcs[i]);
			}
		}
	}

	if(crtc == NULL) {
		swl_error("Crtc Error connector: %d %d\n", conn->connector_id, conn->encoder_id);
		exit(1);
	}
	return crtc;
}

typedef struct {
	uint16_t pixel_clock;
	uint8_t hactive_lsb;
	uint8_t hblank_lsb;
	uint8_t hactive_msb: 4;
	uint8_t hblank_msb: 4;
	uint8_t vactive_lsb;
	uint8_t vblank_lsb;
	uint8_t vactive_msb: 4;
	uint8_t vblank_msb: 4;
	uint8_t hfront_lsb;
	uint8_t hsync_lsb;
	uint8_t todo1;
	uint8_t todo2;
	uint8_t himage_lsb;
	uint8_t vimage_lsb;
	uint8_t todo3;
	uint8_t hborder;
	uint8_t vborder;
	uint8_t features;
}__attribute__((packed)) edid_timing_desc_t;

typedef struct {
	uint8_t red_x_lsb: 2;
	uint8_t red_y_lsb: 2;
	uint8_t green_x_lsb: 2;
	uint8_t green_y_lsb: 2;
	uint8_t blue_x_lsb: 2;
	uint8_t blue_y_lsb: 2;
	uint8_t white_x_lsb: 2;
	uint8_t white_y_lsb: 2;
	uint8_t red_x_msb;
	uint8_t red_y_msb;
	uint8_t green_x_msb;
	uint8_t green_y_msb;
	uint8_t blue_x_msb;
	uint8_t blue_y_msb;
	uint8_t white_x_msb;
	uint8_t white_y_msb;
}__attribute__((packed)) edid_chromaticity_t;

typedef struct {
	uint8_t xres;
	uint8_t image_aspect: 2;
	uint8_t vert_frequency: 6;
}__attribute__((packed)) edid_std_timing_t;

typedef struct {
	uint64_t header_pattern;
	uint16_t be_manufacturer_id;
	uint16_t le_product_code;
	uint32_t serial_number;
	uint8_t manufacture_week;
	uint8_t manufacture_year;
	uint8_t version;
	uint8_t revision;
	uint8_t digital_input: 1;
	uint8_t input_parameters: 7;
	uint8_t hscreensizecm;
	uint8_t vscreensizecm;
	uint8_t gamma;
	uint8_t features;
	edid_chromaticity_t chromaticity;
	uint8_t timing_bitmap_1;
	uint8_t timing_bitmap_2;
	uint8_t timing_bitmap_3;
	edid_std_timing_t timings[8];
	edid_timing_desc_t timing_descriptors[4];
	uint8_t extension_count;
	uint8_t checksum;
}__attribute__((packed)) edid_t;

void swl_output_parse_edid(int fd, drmModeConnector *connector, char **pnp, char **model, int32_t *refresh) {
	drmModePropertyPtr prop;
	drmModePropertyBlobPtr blob;
	edid_t *edid;

	/*PNP should only be 3 characters long
	 * model is a 16 bit int in hex so 
	 * 0xffff at the longest
	 */
	*pnp = calloc(1, 4);
	*model = calloc(1, 7);

	for(uint32_t i = 0; i < connector->count_props; ++i) {
		prop = drmModeGetProperty(fd, connector->props[i]);
		
		if(strcmp(prop->name, "EDID") == 0) {
			blob = drmModeGetPropertyBlob(fd, connector->prop_values[i]);
			edid = blob->data;
			swl_debug("EDID %d\n", blob->length);
			for(uint32_t x = 0; x < blob->length; ++x) {
				swl_log_printf(SWL_LOG_DEBUG, "0x%02x, ", ((uint8_t*)blob->data)[x]);
				if((x + 1) % 16 == 0) {
					swl_log_printf(SWL_LOG_DEBUG, "\n");
				}
			}
			
			/*Set model to product code*/
			snprintf(*model, 7, "0x%04x", edid->le_product_code);
			
			uint16_t manufacturer = (edid->be_manufacturer_id >> 8) | ((edid->be_manufacturer_id & 0xff) << 8);
			/*Decode the PNP of monitor into ASCII*/
			(*pnp)[2] = ((manufacturer >> 0) & 0x1f) + ('A' - 1);
			(*pnp)[1] = ((manufacturer >> 5) & 0x1f) + ('A' - 1);
			(*pnp)[0] = ((manufacturer >> 10) & 0x1f) + ('A' - 1);
			drmModeFreePropertyBlob(blob);
		}
	
		drmModeFreeProperty(prop);
	}
}

void swl_output_init_common(int fd, drmModeConnector *connector, swl_output_t *output, int32_t x, int32_t y) {
	output->description = calloc(1, 256);
	output->name = calloc(1, 64);

	output->mode.refresh = connector->modes->vrefresh;
	output->mode.width = connector->modes->hdisplay;
	output->mode.flags = WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.height = connector->modes->vdisplay;
	output->x = x;
	output->y = y;
	output->scale = 1;
	output->tranform = WL_OUTPUT_TRANSFORM_NORMAL;
	output->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->height = connector->mmHeight;
	output->width = connector->mmWidth;
	
	swl_output_parse_edid(fd, connector, &output->make, &output->model, &output->mode.refresh);

	snprintf(output->name, 64, "%s-%d", drmModeGetConnectorTypeName(connector->connector_type), connector->connector_type_id);
	snprintf(output->description, 256, "%s %s (%s)", output->name, output->make, output->model);	
	
	//output->draw_texture = render_surface_texture; 
	wl_signal_init(&output->frame);
	wl_signal_init(&output->destroy);
	wl_signal_init(&output->bind);
}

swl_drm_output_t *swl_drm_output_create(struct gbm_device *gbm, int fd, drmModeRes *res, drmModeConnector *conn, swl_renderer_t *renderer, int32_t x, int32_t y) {
	swl_drm_output_t *output = calloc(1, sizeof(swl_drm_output_t));
	uint32_t width = conn->modes->hdisplay;
	uint32_t height = conn->modes->vdisplay;
	uint32_t ret = 0;
	output->connector = conn;
	swl_output_init_common(fd, conn, &output->common, x, y);
	output->original_crtc = swl_drm_get_conn_crtc(fd, conn, res);

	output->common.renderer = renderer;
	output->common.buffer = calloc(2, sizeof(swl_gbm_buffer_t*));
	output->common.buffer[0] = swl_gbm_buffer_create(gbm, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING, 32);
	output->common.buffer[1] = swl_gbm_buffer_create(gbm, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING, 32);
	output->cursor = swl_gbm_buffer_create(gbm, 64, 64, GBM_FORMAT_XRGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING, 32);
	
	output->common.targets = calloc(2, sizeof(swl_renderer_target_t*));
	output->common.targets[0] = output->common.renderer->create_target(output->common.renderer, output->common.buffer[0]);
	output->common.targets[1] = output->common.renderer->create_target(output->common.renderer, output->common.buffer[1]);

	output->cursor_target =	output->common.renderer->create_target(output->common.renderer, output->cursor);
	output->common.renderer->attach_target(output->common.renderer, output->cursor_target);
	output->common.renderer->begin(output->common.renderer);
	output->common.renderer->clear(output->common.renderer, 1.0f, 0.0f, 0.0f, 0.0f);
	output->common.renderer->end(output->common.renderer);

	ret = drmModeAddFB(fd, width, height, 24, 32, output->common.buffer[0]->pitch, output->common.buffer[0]->handle, &output->fb_id[0]);
	ret = drmModeAddFB(fd, width, height, 24, 32, output->common.buffer[1]->pitch, output->common.buffer[1]->handle, &output->fb_id[1]);

	return output;
}

void swl_drm_outputs_destroy(int fd, struct wl_list *list) {
	swl_drm_output_t *output, *tmp;
	wl_list_for_each_safe(output, tmp, list, link) {
		wl_signal_emit(&output->common.destroy, &output);
	
		drmModeRmFB(fd, output->fb_id[0]);
		drmModeRmFB(fd, output->fb_id[1]);

		swl_gbm_buffer_destroy(output->common.buffer[0]);
		swl_gbm_buffer_destroy(output->common.buffer[1]);
		swl_gbm_buffer_destroy(output->cursor);

		free(output->common.buffer);
		output->common.renderer->destroy_target(output->common.renderer, output->common.targets[0]);
		output->common.renderer->destroy_target(output->common.renderer, output->common.targets[1]);
		output->common.renderer->destroy_target(output->common.renderer, output->cursor_target);	
		free(output->common.targets);

		drmModeFreeCrtc(output->original_crtc);
		drmModeFreeConnector(output->connector);
		wl_list_remove(&output->link);
		
		free(output->common.name);
		free(output->common.description);
		free(output->common.make);
		free(output->common.model);
		free(output);
	}
}

int drm_create_outputs(struct gbm_device *gbm, int fd, drmModeRes *res, swl_renderer_t *renderer, struct wl_list *list) {
	uint32_t count;
	drmModeConnector *connector = NULL;
	swl_drm_output_t *output;
	int32_t x = 0;
	int32_t y = 0;

	for(count = 0; count < res->count_connectors; count++) {
		connector = drmModeGetConnector(fd, res->connectors[count]);
		if(!connector) {
			swl_warn("Failed to get connector %d\n", res->connectors[count]);
			continue;
		} else if(connector->connection == DRM_MODE_CONNECTED) {
			output = swl_drm_output_create(gbm, fd, res, connector, renderer, x, y);
			wl_list_insert(list, &output->link);
			x = output->common.x + output->common.mode.width;

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
	if(version >= WL_OUTPUT_GEOMETRY_SINCE_VERSION) {
		wl_output_send_geometry(resource,
				output->common.x, output->common.y, 
				output->common.width, output->common.height, 
				output->common.subpixel, output->common.make, 
				output->common.model, output->common.tranform);
	}
	if(version >= WL_OUTPUT_MODE_SINCE_VERSION) {
		wl_output_send_mode(resource, output->common.mode.flags, 
				output->common.mode.width, output->common.mode.height,
				output->common.mode.refresh);
	}
	if(version >= WL_OUTPUT_SCALE_SINCE_VERSION) {
		wl_output_send_scale(resource, output->common.scale);
	}
	if(version >= WL_OUTPUT_NAME_SINCE_VERSION) {
		wl_output_send_name(resource, output->common.name);
	}
	if(version >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION) {
		wl_output_send_description(resource, output->common.description);
	}
	if(version >= WL_OUTPUT_DONE_SINCE_VERSION) {
		wl_output_send_done(resource);
	}

	wl_signal_emit(&output->common.bind, resource);
}

static void modeset_page_flip_event(int fd, unsigned int frame,
				    unsigned int sec, unsigned int usec,
				    void *data) {
	swl_drm_output_t *output = data;

	output->pending = false;
	if (!output->shutdown) {
		wl_signal_emit(&output->common.frame, output);

		if(drmModePageFlip(fd, output->original_crtc->crtc_id, 
				output->fb_id[output->common.front_buffer], 
				DRM_MODE_PAGE_FLIP_EVENT, data)) {
			swl_error("Page Flip Failed\n");
		}
		output->common.front_buffer ^= 1;
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
	wl_list_for_each(output, &drm->outputs, link) {
		output->common.renderer = drm->renderer;
		output->common.global = wl_global_create(drm->display, &wl_output_interface, 
				SWL_OUTPUT_VERSION, output, swl_output_bind);
		drmModeSetCrtc(drm->fd, output->original_crtc->crtc_id, output->fb_id[0], 
				0, 0, &output->connector->connector_id, 1, output->connector->modes);
		output->common.front_buffer ^= 1;
		drmModeSetCursor(drm->fd, output->original_crtc->crtc_id, output->cursor->handle,
				output->cursor->width, output->cursor->height);
		drmModePageFlip(drm->fd, output->original_crtc->crtc_id, 
			output->fb_id[output->common.front_buffer], DRM_MODE_PAGE_FLIP_EVENT, output);
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

int swl_drm_backend_move_cursor(swl_display_backend_t *display, int32_t x, int32_t y) {
	swl_drm_backend_t *drm = (swl_drm_backend_t*)display;
	swl_drm_output_t *output;
	wl_list_for_each(output, &drm->outputs, link) {
		if((x >= output->common.x - 64 && x <= output->common.x + output->common.mode.width + 64) &&
				(y >= output->common.y - 64 && y <= output->common.y + output->common.mode.height + 64)) {
			drmModeMoveCursor(drm->fd, output->original_crtc->crtc_id, x - output->common.x, y - output->common.y);
		}
	}
	return 0;
}

void swl_drm_activate(struct wl_listener *listener, void *data) {
	swl_drm_backend_t *drm = wl_container_of(listener, drm, activate);
	swl_drm_output_t *output;
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;

	wl_list_for_each(output, &drm->outputs, link) {
		output->shutdown = false;
		drmModeSetCrtc(drm->fd, output->original_crtc->crtc_id, output->fb_id[0], 
				0, 0, &output->connector->connector_id, 1, output->connector->modes);
		output->common.front_buffer ^= 1;
		drmModeSetCursor(drm->fd, output->original_crtc->crtc_id, output->cursor->handle,
				output->cursor->width, output->cursor->height);
		drmModePageFlip(drm->fd, output->original_crtc->crtc_id, 
			output->fb_id[output->common.front_buffer], DRM_MODE_PAGE_FLIP_EVENT, output);
		output->pending = true;
	}
}

void swl_drm_backend_destroy(swl_display_backend_t *display, swl_session_backend_t *session) {
	swl_drm_backend_t *drm = (void *)display;
	
	wl_event_source_remove(drm->readable);

	swl_drm_outputs_destroy(drm->fd, &drm->outputs);
	gbm_device_destroy(drm->gbm);
	drm->renderer->destroy(drm->renderer);

	session->close_dev(session, drm->dev);
	close(drm->fd);
	free(drm);
}

swl_renderer_t *swl_drm_backend_get_renderer(swl_display_backend_t *display) {
	swl_drm_backend_t *drm = (swl_drm_backend_t*)display;

	return drm->renderer;
}

swl_display_backend_t *swl_drm_create_backend(struct wl_display *display, swl_session_backend_t *session, const char *drm_device) {
	swl_drm_backend_t *drm;
	drmModeResPtr res;
	struct wl_event_loop *loop = wl_display_get_event_loop(display);

	drm = calloc(1, sizeof(swl_drm_backend_t));
	drm->dev = session->open_dev(session, drm_device, &drm->fd);

	res = drmModeGetResources(drm->fd);

	wl_list_init(&drm->outputs);
	drm->gbm = gbm_create_device(drm->fd);

	drm->renderer = swl_egl_renderer_create_by_fd(drm->fd);
	if(drm->renderer == NULL) {
		return NULL;
	}

	drm_create_outputs(drm->gbm, drm->fd, res, drm->renderer, &drm->outputs);
	drmModeFreeResources(res);
	drm->display = display;
	wl_signal_init(&drm->common.new_output);

	drm->activate.notify = swl_drm_activate;
	drm->deactivate.notify = swl_drm_deactivate;

	wl_signal_add(&session->activate, &drm->activate);
	wl_signal_add(&session->disable, &drm->deactivate);

	drm->readable = wl_event_loop_add_fd(loop, drm->fd, WL_EVENT_READABLE,
			swl_drm_readable, drm);


	drm->common.SWL_DISPLAY_BACKEND_DESTROY = swl_drm_backend_destroy;
	drm->common.SWL_DISPLAY_BACKEND_START = swl_drm_backend_start;
	drm->common.SWL_DISPLAY_BACKEND_STOP = swl_drm_backend_stop;
	drm->common.SWL_DISPLAY_BACKEND_GET_RENDERER = swl_drm_backend_get_renderer;
	drm->common.SWL_DISPLAY_BACKEND_MOVE_CURSOR = swl_drm_backend_move_cursor;
	return (swl_display_backend_t*)drm;
}
