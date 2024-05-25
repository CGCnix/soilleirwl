/*Local*/
#include "./xdg-shell.h"

/*wayland*/
#include <wayland-server.h>

/*Xkbcommon*/
#include <xkbcommon/xkbcommon.h>

/*libdrm*/
#include <xf86drmMode.h>
#include <xf86drm.h>

/*Linux EVDev*/
#include <linux/input.h>
#include <linux/input-event-codes.h>

/*Std c library functions*/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef struct swl_buffer {
	uint32_t handle, fb_id;
	uint32_t format, height, width, pitch;
	size_t size, offset;

	uint8_t *data;
} swl_buffer_t;

typedef struct swl_seat {
	const char *seat_name;
	uint32_t caps;

	struct xkb_context *xkb;
} swl_seat_t;

typedef struct {
	void *data;
	uint32_t height, width, stride;
} swl_texture_t;

typedef struct {
	struct wl_resource *buffer;
	struct wl_resource *surface;
	struct wl_resource *shell_surface;
	swl_texture_t texture;
} swl_surface_t;

typedef struct swl_xdg_toplevel swl_xdg_toplevel_t;

typedef struct swl_backend {
	/*Display Backend*/
	int drmfd;
	drmModeConnectorPtr connector;
	drmModeCrtcPtr saved_crtc;

	int pending, shutdown;

	swl_buffer_t buffers[2];
	int front_bo;
	/*keyboard*/
	int keyboardfd;

	struct wl_list clients;
	swl_xdg_toplevel_t *active;
	swl_seat_t seat;

	/*wayland*/
	struct wl_display *display;
	struct wl_event_source *key;
	struct wl_event_source *drm;
} swl_backend_t;

typedef struct swl_client {
	struct wl_client *client; /*Used to ID*/
	
	struct wl_list surfaces; /*Surfaces as a client may have multiple*/
	struct wl_resource *keyboard; /*If one exists*/

	struct wl_list link;
} swl_client_t;


typedef struct swl_xdg_surface {
	swl_surface_t *swl_surface;
	
	swl_backend_t *backend;
} swl_xdg_surface_t;

struct swl_xdg_toplevel {
	swl_client_t *client;
	swl_xdg_surface_t *swl_xdg_surface;
	
	struct wl_list link;
};

drmModeConnector *drm_get_first_connector(int fd, drmModeRes *res) {
	uint32_t count;
	drmModeConnector *connector = NULL;

	for(count = 0; count < res->count_connectors; count++) {
		connector = drmModeGetConnector(fd, res->connectors[count]);
		if(!connector) {
			printf("Failed to get connector %d\n", res->connectors[count]);
			continue;
		} else if(connector->connection == DRM_MODE_CONNECTED) {
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	return connector;
}

drmModeCrtc *drm_get_conn_crtc(int fd, drmModeConnector *conn, drmModeRes *res) {
	drmModeEncoder *encoder = NULL;
	drmModeCrtc *crtc = NULL;

	if(conn->encoder_id) { /*There is already an encoder setup just use that*/
		encoder = drmModeGetEncoder(fd, conn->encoder_id);
	}

	if(encoder || encoder->crtc_id) {
		crtc = drmModeGetCrtc(fd, encoder->crtc_id);
	}

	return crtc;
}

int swl_drm_create_fb(int fd, swl_buffer_t *bo, uint32_t width, uint32_t height) {
	int ret;

	if(!bo) {
		printf("Invalid input create fb\n");
		return -1;
	}

	bo->width = width;
	bo->height = height;

	ret = drmModeCreateDumbBuffer(fd, width, height, 32, 0, &bo->handle, &bo->pitch, &bo->size);
	if(ret) {
		printf("Unable to create drmModeCreateDumbBuffer\n");
		return -1;
	}

	ret = drmModeAddFB(fd, width, height, 24, 32, bo->pitch, bo->handle, &bo->fb_id);
	if(ret) {
		printf("Unable to add fb to drm card\n");
		goto error_destroy;
	}

	ret = drmModeMapDumbBuffer(fd, bo->handle, &bo->offset);
	if(ret) {
		printf("Unable to map dumb buffer\n");
		goto error_fb;
	}

	bo->data = mmap(NULL, bo->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, bo->offset);
	if(bo->data == MAP_FAILED) {
		printf("MMAP failed\n");
		goto error_fb;
	}
	return 0;

	error_fb:
	drmModeRmFB(fd, bo->fb_id);
	error_destroy:
	drmModeDestroyDumbBuffer(fd, bo->handle);
	return ret;
}

static void render_surface_texture(swl_surface_t *surface, swl_buffer_t *buffer) {
	uint32_t *dst = (uint32_t*)buffer->data;

	if(surface->texture.data) {
		for(uint32_t y = 0; y < surface->texture.height; y++) {
			for(uint32_t x = 0; x < surface->texture.width; x++) {
				dst[y * buffer->width + x] = ((uint32_t*)surface->texture.data)[y * surface->texture.width + x];
			}
		}
	}
}

static void modeset_page_flip_event(int fd, unsigned int frame,
				    unsigned int sec, unsigned int usec,
				    void *data) {
	swl_backend_t *backend = data;
	swl_xdg_toplevel_t *toplevel;
	swl_client_t *client;

	backend->pending = false;
	if (!backend->shutdown) {
		wl_list_for_each(client, &backend->clients, link) {
			wl_list_for_each(toplevel, &client->surfaces, link) {
				if(toplevel->swl_xdg_surface->swl_surface->buffer) {
					render_surface_texture(toplevel->swl_xdg_surface->swl_surface,
							&backend->buffers[backend->front_bo]);
				}
			}
		}

		drmModePageFlip(fd, backend->saved_crtc->crtc_id, 
				backend->buffers[backend->front_bo].fb_id, 
				DRM_MODE_PAGE_FLIP_EVENT, data);
		backend->front_bo ^= 1;
		backend->pending = true;
	}
}

int swl_drm_readable(int fd, uint32_t mask, void *data) {
	swl_backend_t *backend = data;
	drmEventContext ev;
	ev.version = 2;
	ev.page_flip_handler = modeset_page_flip_event;

	drmHandleEvent(fd, &ev);

	return 0;
}

void swl_send_key(struct wl_resource *keyboard, uint32_t value, uint32_t code) {
	if(code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
		wl_keyboard_send_modifiers(keyboard, 0, 1 * value, 0, 0, 0);
	}
	wl_keyboard_send_key(keyboard, 0, 0, code, value);
}

int swl_key_readable(int fd, uint32_t mask, void *data) {
	swl_backend_t *backend = data;
	struct input_event li;

	read(fd, &li, 24);

	if(li.type == EV_KEY) {
		if(li.code == 0x10) { 
			wl_display_terminate(backend->display);
		}
		if(backend->active && backend->active->client->keyboard) {
			swl_send_key(backend->active->client->keyboard, li.value, li.code);
		}
	}

	return 0;
}

swl_backend_t *backend_create(struct wl_display *display) {
	swl_backend_t *backend = calloc(1, sizeof(swl_backend_t));
	const char *drm_card, *keyboard;
	struct wl_event_loop *loop;
	drmModeRes *res;

	drm_card = "/dev/dri/card0";
	keyboard = getenv("SWL_KEYBOARD");
	if(getenv("SWL_DRM_OVERRIDE")) {
		drm_card = getenv("SWL_DRM_OVERRIDE");
	}

	if(!keyboard) {
		return NULL;
	}

	loop = wl_display_get_event_loop(display);

	backend->drmfd = open(drm_card, O_RDWR | O_CLOEXEC);
	res = drmModeGetResources(backend->drmfd);

	backend->connector = drm_get_first_connector(backend->drmfd, res);
	backend->saved_crtc = drm_get_conn_crtc(backend->drmfd, backend->connector, res);

	swl_drm_create_fb(backend->drmfd, &backend->buffers[0], 1920, 1080);
	memset(backend->buffers[0].data, 0xaa, backend->buffers[0].size);

	swl_drm_create_fb(backend->drmfd, &backend->buffers[1], 1920, 1080);
	memset(backend->buffers[1].data, 0xaa, backend->buffers[1].size);

	drmModeSetCrtc(backend->drmfd, backend->saved_crtc->crtc_id, backend->buffers[0].fb_id, 
			0, 0, &backend->connector->connector_id, 1, backend->connector->modes);
	backend->front_bo ^= 1;

	backend->drm = wl_event_loop_add_fd(loop, backend->drmfd, WL_EVENT_READABLE, swl_drm_readable, backend);
	printf("%d\n", backend->front_bo);
	
	/*Do input stuffs*/
	backend->keyboardfd = open(keyboard, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		

	backend->key = wl_event_loop_add_fd(loop, backend->keyboardfd, WL_EVENT_READABLE, swl_key_readable, backend);

	backend->display = display;
	
	backend->seat.seat_name = "seat0";
	backend->seat.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	backend->seat.caps = WL_SEAT_CAPABILITY_KEYBOARD;
	wl_list_init(&backend->clients);

	return backend;
}

void backend_destroy(swl_backend_t *backend) {
	drmModeSetCrtc(backend->drmfd, backend->saved_crtc->crtc_id, 
			backend->saved_crtc->buffer_id, 0, 0, &backend->connector->connector_id, 
			1, &backend->saved_crtc->mode);

	close(backend->drmfd);
}

void wl_surface_destroy(struct wl_client *client, struct wl_resource *resource) {

}

void wl_surface_commit(struct wl_client *client, struct wl_resource *resource) {
	struct wl_shm_buffer *buffer;
	swl_surface_t *surface = wl_resource_get_user_data(resource);
	if(!surface->buffer) {
		xdg_surface_send_configure(surface->shell_surface, 0);
		return;
	}

	if(surface->texture.data) {
		free(surface->texture.data);
		surface->texture.data = NULL;
	}

	buffer = wl_shm_buffer_get(surface->buffer);
	surface->texture.width = wl_shm_buffer_get_width(buffer);
	surface->texture.height = wl_shm_buffer_get_height(buffer);
	surface->texture.stride = wl_shm_buffer_get_stride(buffer);
	surface->texture.data = calloc(1, surface->texture.stride * surface->texture.height);
	void *data = wl_shm_buffer_get_data(buffer);

	memcpy(surface->texture.data, data, surface->texture.stride * surface->texture.height);
	wl_buffer_send_release(surface->buffer);
}

void wl_surface_set_opaque_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}

void wl_surface_set_input_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}


void wl_surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
	struct wl_resource *callback = wl_resource_create(client, &wl_callback_interface, 1, id);
	wl_resource_set_implementation(callback, NULL, NULL, NULL);
	wl_callback_send_done(callback, 0);
}

void wl_surface_set_buffer_scale(struct wl_client *client, 
		struct wl_resource *resource, int32_t scale) {

}

void wl_surface_set_buffer_transform(struct wl_client *client, struct wl_resource *resource, int32_t transform) {

}

void wl_surface_attach(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *buffer, int32_t x, int32_t y) {
	swl_surface_t *surface = wl_resource_get_user_data(resource);
	surface->buffer = buffer;
}

void wl_surface_damage(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void wl_surface_damage_buffer(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void wl_surface_offset(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y) {

}

static const struct wl_surface_interface surface_implementation = {
		.set_opaque_region = wl_surface_set_opaque_region,
		.damage = wl_surface_damage,
		.damage_buffer = wl_surface_damage_buffer,
		.offset = wl_surface_offset,
		.set_buffer_scale = wl_surface_set_buffer_scale,
		.attach = wl_surface_attach,
		.frame = wl_surface_frame,
		.commit = wl_surface_commit,
		.destroy = wl_surface_destroy,
		.set_input_region = wl_surface_set_input_region,
		.set_buffer_transform = wl_surface_set_buffer_transform,
};

static void wl_compositor_create_surface(struct wl_client *client, struct wl_resource *compositor, uint32_t id) {
	struct wl_resource *surface;
	swl_surface_t *swl_surface = calloc(1, sizeof(swl_surface_t));
	
	surface = wl_resource_create(client, &wl_surface_interface, 6, id);
	wl_resource_set_implementation(surface, &surface_implementation, swl_surface, NULL);

	swl_surface->surface = surface;
}

static void wl_compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	wl_client_post_implementation_error(client, "This isn't implemented\n");
}

static const struct wl_compositor_interface compositor_interface = {
	.create_region = wl_compositor_create_region,
	.create_surface = wl_compositor_create_surface,
};

static void wl_compositor_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	


	resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &compositor_interface, data, NULL);
}

void swl_xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_unset_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_minimized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_fullscreen(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *output) {

}

void swl_xdg_toplevel_set_parent(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *parent) {

}

void swl_xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *toplevel, 
		const char *title) {
	printf("Title set to %s\n", title);
}

void swl_xdg_toplevel_set_appid(struct wl_client *client, struct wl_resource *toplevel, 
		const char *appid) {

}

void swl_xdg_toplevel_set_max_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

void swl_xdg_toplevel_set_min_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

void swl_xdg_toplevel_show_window_menu(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {

}

void swl_xdg_toplevel_move(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial) {

}

void swl_xdg_toplevel_resize(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial, uint32_t edges) {

}

static const struct xdg_toplevel_interface xdg_toplevel_impl = {
	.resize = swl_xdg_toplevel_resize,
	.move = swl_xdg_toplevel_move,
	.destroy = swl_xdg_toplevel_destroy,
	.show_window_menu = swl_xdg_toplevel_show_window_menu,
	.set_max_size = swl_xdg_toplevel_set_max_size,
	.set_min_size = swl_xdg_toplevel_set_min_size,
	.set_app_id = swl_xdg_toplevel_set_appid,
	.set_title = swl_xdg_toplevel_set_title,
	.set_parent = swl_xdg_toplevel_set_parent,
	.unset_fullscreen = swl_xdg_toplevel_unset_fullscreen,
	.set_fullscreen = swl_xdg_toplevel_set_fullscreen,
	.set_minimized = swl_xdg_toplevel_set_minimized,
	.set_maximized = swl_xdg_toplevel_set_maximized,
	.unset_maximized = swl_xdg_toplevel_unset_maximized,
};

void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
}

void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
		uint32_t id, struct wl_resource *xdg_surface, struct wl_resource *xdg_positioner) {

}

swl_client_t *swl_get_client(struct wl_client *client, struct wl_list *list) {
	swl_client_t *output;
	wl_list_for_each(output, list, link) {
		if(client == output->client) {
			return output;
		}
	}
	return NULL;
}

swl_client_t *swl_get_client_or_create(struct wl_client *client, struct wl_list *list) {
	swl_client_t *output;
	wl_list_for_each(output, list, link) {
		if(client == output->client) {
			return output;
		}
	}
	
	output = calloc(1, sizeof(swl_client_t));
	output->client = client;
	wl_list_insert(list, &output->link);
	wl_list_init(&output->surfaces);
	return output;
}



void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface,
		uint32_t id) {
	struct wl_resource *resource;
	struct swl_client *swl_client;
	swl_xdg_toplevel_t *swl_xdg_toplevel = calloc(1, sizeof(swl_xdg_toplevel_t));
	
	swl_xdg_toplevel->swl_xdg_surface = wl_resource_get_user_data(xdg_surface);

	resource = wl_resource_create(client, &xdg_toplevel_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_toplevel_impl, swl_xdg_toplevel, NULL);
	
	swl_client = swl_get_client_or_create(client, &swl_xdg_toplevel->swl_xdg_surface->backend->clients);
	wl_list_insert(&swl_client->surfaces, &swl_xdg_toplevel->link);

	swl_xdg_toplevel->client = swl_client;
	swl_xdg_toplevel->swl_xdg_surface->backend->active = swl_xdg_toplevel;

	struct wl_array array;
	wl_array_init(&array);

	xdg_toplevel_send_configure(resource, 960, 1080, &array);
}

void xdg_surface_set_geometry(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource,
		uint32_t serial) {

}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy = xdg_surface_destroy,
	.set_window_geometry = xdg_surface_set_geometry,
	.get_popup = xdg_surface_get_popup,
	.get_toplevel = xdg_surface_get_toplevel,
	.ack_configure = xdg_surface_ack_configure,
};

void xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {

}

void xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {

}

void xdg_wm_base_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

void xdg_wm_base_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface) {
	struct wl_resource *xdg_surface_resource;
	swl_surface_t *swl_surface;
	swl_xdg_surface_t *swl_xdg_surface = calloc(1, sizeof(swl_xdg_surface_t));

	swl_surface = wl_resource_get_user_data(surface);
	swl_xdg_surface->swl_surface = swl_surface;
	swl_xdg_surface->backend = wl_resource_get_user_data(resource);

	xdg_surface_resource = wl_resource_create(client, &xdg_surface_interface, 6, id);
	swl_surface->shell_surface = xdg_surface_resource;
	wl_resource_set_implementation(xdg_surface_resource, &xdg_surface_impl, swl_xdg_surface, NULL);
}


static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
		.pong = xdg_wm_base_pong,
		.get_xdg_surface = xdg_wm_base_create_surface,
		.destroy = xdg_wm_base_destroy,
		.create_positioner = xdg_wm_base_positioner,
};

static void xdg_wm_base_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	
	resource = wl_resource_create(client, &xdg_wm_base_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_wm_base_implementation, data, NULL);
}

void swl_keyboard_release(struct wl_client *client, struct wl_resource *resource) {

}

static const struct wl_keyboard_interface wl_keyboard_impl = {
	.release = swl_keyboard_release,
};

void wl_seat_release(struct wl_client *client, struct wl_resource *resource) {

}

void wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *keyboard;
	swl_client_t *swl_client;
	swl_backend_t *backend = wl_resource_get_user_data(resource);
	char tmp[] = "/tmp/swlkeyfd-XXXXXX";

	swl_client = swl_get_client_or_create(client, &backend->clients);

	keyboard = wl_resource_create(client, &wl_keyboard_interface, 9, id);
	wl_resource_set_implementation(keyboard, &wl_keyboard_impl, NULL, NULL);

	struct xkb_keymap *map = xkb_keymap_new_from_names(backend->seat.xkb, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	char *map_str = xkb_keymap_get_as_string(map, XKB_KEYMAP_FORMAT_TEXT_V1);
	int fd = mkstemp(tmp);
	ftruncate(fd, strlen(map_str));
	write(fd, map_str, strlen(map_str));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(map_str));
	unlink(tmp);
	close(fd);

	struct wl_array array;
	wl_array_init(&array);
	swl_xdg_toplevel_t *toplevel;

	wl_list_for_each(toplevel, &swl_client->surfaces, link) {
		wl_keyboard_send_enter(keyboard, 0, toplevel->swl_xdg_surface->swl_surface->surface, &array);
	}
	swl_client->keyboard = keyboard;
}

void wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

void wl_seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}

static const struct wl_seat_interface wl_seat_impl = {
	.release = wl_seat_release,
	.get_pointer = wl_seat_get_pointer, 
	.get_keyboard = wl_seat_get_keyboard,
	.get_touch = wl_seat_get_touch,
};

static void wl_seat_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	swl_backend_t *backend = data;
	resource = wl_resource_create(client, &wl_seat_interface, 9, id);
	wl_resource_set_implementation(resource, &wl_seat_impl, data, NULL);

	wl_seat_send_name(resource, backend->seat.seat_name);
	wl_seat_send_capabilities(resource, backend->seat.caps);
}

int main(int argc, char **argv) {
	struct wl_display *display = wl_display_create();
	wl_display_add_socket_auto(display);
	swl_backend_t *backend = backend_create(display);

	drmModePageFlip(backend->drmfd, backend->saved_crtc->crtc_id, 
			backend->buffers[backend->front_bo].fb_id, DRM_MODE_PAGE_FLIP_EVENT, backend);
	wl_display_init_shm(display);
	wl_global_create(display, &wl_seat_interface, 9, backend, wl_seat_bind);
	wl_global_create(display, &wl_compositor_interface, 6, display, wl_compositor_bind);
	wl_global_create(display, &xdg_wm_base_interface, 6, backend, xdg_wm_base_bind);
	
	wl_display_run(display);

	backend_destroy(backend);

	return 0;
}
