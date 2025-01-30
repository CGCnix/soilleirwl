#include "soilleirwl/allocator/gbm.h"
#include "soilleirwl/backend/backend.h"
#include "soilleirwl/interfaces/swl_input_device.h"
#include <soilleirwl/interfaces/swl_output.h>
#include <soilleirwl/logger.h>
#include <soilleirwl/renderer.h>
#include <gbm.h>
#include <soilleirwl/backend/xcb.h>

#include <linux/input-event-codes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-server-core.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <xcb/dri3.h>
#include <xcb/present.h>

typedef struct swl_x11_output {
	swl_output_t common;
	
	xcb_window_t window;
	xcb_gcontext_t gc;
	xcb_pixmap_t pixmaps[2];
	struct gbm_device *dev;
} swl_x11_output_t;

typedef struct swl_x11_backend {
	swl_backend_t backend;
	struct wl_display *display;
	xcb_connection_t *connection;
	xcb_screen_t *screen;
	int drm_fd;

	swl_x11_output_t *output;
	swl_input_dev_t input;

	struct wl_signal new_output;
	struct wl_signal new_input;
	struct wl_signal activate;
	struct wl_signal disable;

	struct wl_event_source *event;
} swl_x11_backend_t;

static int16_t swl_xcb_button_to_linux(uint32_t detail) {
	switch(detail) {
		case XCB_BUTTON_INDEX_1: return BTN_LEFT;
		case XCB_BUTTON_INDEX_2: return BTN_MIDDLE;
		case XCB_BUTTON_INDEX_3: return BTN_RIGHT;
		default: return BTN_RIGHT;
	}
}

static void swl_output_release(struct wl_client *client, struct wl_resource *resource) {

}

static struct wl_output_interface swl_output_impl = {
	.release = swl_output_release,
};

static void swl_output_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	swl_x11_output_t *output = data;

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
}

int swl_x11_create_fb(struct gbm_device *dev, struct gbm_bo **gbm_bo, swl_buffer_t *bo, uint32_t width, uint32_t height) {
	*gbm_bo = gbm_bo_create(dev, width, height, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
	bo->render = gbm_device_get_fd(dev);
	bo->height = gbm_bo_get_height(*gbm_bo);
	bo->width = gbm_bo_get_width(*gbm_bo);
	bo->pitch = gbm_bo_get_stride(*gbm_bo);
	bo->handle = gbm_bo_get_handle(*gbm_bo).u32;
	bo->offset = 0;
	bo->size = bo->height * bo->pitch;
	return 0;
}

int swl_x11_destroy_fb(struct gbm_bo **gbm_bo, swl_buffer_t *bo) {
	gbm_bo_destroy(*gbm_bo);

	return 0;
}

swl_x11_output_t *swl_x11_output_create(swl_x11_backend_t *x11) {
	swl_x11_output_t *out = calloc(1, sizeof(swl_x11_output_t));
	
	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_BIT_GRAVITY;
	uint32_t values[3];

	values[0] = 0x000000;
	values[1] = XCB_GRAVITY_STATIC;
	values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_FOCUS_CHANGE;

	out->window = xcb_generate_id(x11->connection);
	xcb_create_window(x11->connection, 24, out->window, x11->screen->root, 
			0, 0, 640, 480, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT, x11->screen->root_visual,
			mask, values);

	uint32_t evid = xcb_generate_id(x11->connection);

	xcb_map_window(x11->connection, out->window);
	xcb_flush(x11->connection);
	xcb_generic_error_t *err;

	xcb_dri3_open_cookie_t cookie = xcb_dri3_open(x11->connection, out->window, 0);
	xcb_dri3_open_reply_t *reply = xcb_dri3_open_reply(x11->connection, cookie, &err);
	int *fds = xcb_dri3_open_reply_fds(x11->connection, reply);
	
	if(err) {
		swl_debug("X11 err\n");
	}

	x11->drm_fd = fds[0];
	swl_debug("X11 FD %d\n", fds[0]);
	int flags = fcntl(x11->drm_fd, F_GETFD);
	fcntl(x11->drm_fd, F_SETFD, flags | FD_CLOEXEC);

	x11->output = out;
	x11->output->common.model = "x11";
	x11->output->common.make = "x11";
	x11->output->common.name = "x11";
	x11->output->common.description = "x11";
	x11->output->common.width = 640;
	x11->output->common.height = 480;
	x11->output->common.scale = 1;
	x11->output->common.mode.flags = 1;
	x11->output->common.mode.refresh = 60;

	out->common.mode.width = 640;
	out->common.mode.height = 480;
	out->common.renderer = swl_egl_renderer_create_by_fd(fds[0]);
	x11->output->dev = gbm_create_device(fds[0]);
	
	xcb_present_select_input(x11->connection, evid, out->window, XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY);

	x11->output->common.buffer = calloc(2, sizeof(swl_gbm_buffer_t*));
	x11->output->common.targets = calloc(2, sizeof(swl_renderer_target_t*));
	
	x11->output->common.buffer[0] = swl_gbm_buffer_create(x11->output->dev, out->common.mode.width, out->common.mode.height, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR, 32);
	x11->output->common.buffer[1] = swl_gbm_buffer_create(x11->output->dev, out->common.mode.width, out->common.mode.height, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR, 32);
	x11->output->common.targets[0] = x11->output->common.renderer->create_target(x11->output->common.renderer, x11->output->common.buffer[0]);
	x11->output->common.targets[1] = x11->output->common.renderer->create_target(x11->output->common.renderer, x11->output->common.buffer[1]);

	for(uint32_t i = 0; i < 2; i++) {
		x11->output->pixmaps[i] = xcb_generate_id(x11->connection);
		xcb_dri3_pixmap_from_buffer(x11->connection, x11->output->pixmaps[i], x11->output->window, x11->output->common.buffer[i]->size, out->common.mode.width, out->common.mode.height, x11->output->common.buffer[i]->pitch, 24, 32, gbm_bo_get_fd(x11->output->common.buffer[i]->bo));
	}

	wl_signal_init(&out->common.destroy);
	wl_signal_init(&out->common.frame);

	x11->output->common.global = wl_global_create(x11->display, &wl_output_interface, SWL_OUTPUT_VERSION, out, swl_output_bind);
	free(reply);
	return out;
}

int swl_x11_event(int fd, uint32_t mask, void *data) {
	swl_x11_backend_t *x11 = data;
	static int32_t py;
	static int32_t px;
	if(mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		wl_display_terminate(x11->display);
	}

	xcb_generic_event_t *ev = xcb_poll_for_event(x11->connection);
	while(ev) {
		switch(ev->response_type) {
			case XCB_EXPOSE: {
				break;
			}
			case XCB_GRAPHICS_EXPOSURE: {
				break;
			}
			case XCB_NO_EXPOSURE: {
				return 0;
				break;
			}
			case XCB_GE_GENERIC: {
			xcb_ge_generic_event_t *ge = (void*)ev;
			if(ge->event_type == XCB_PRESENT_EVENT_COMPLETE_NOTIFY) {
			wl_signal_emit(&x11->output->common.frame, x11->output);
			xcb_present_pixmap(x11->connection, 
				x11->output->window, 
				x11->output->pixmaps[x11->output->common.front_buffer],
				0,
				0,
				0,
				0,
				0,
				0,
				0,
				0,
				XCB_PRESENT_OPTION_NONE,
				0,
				0,
				0,
				0, NULL);
			}

				break;
			}
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *kp = (void*)ev;
				swl_key_event_t key;
				key.state = 1;
				key.key = kp->detail - 8;
				wl_signal_emit(&x11->input.key, &key);
				break;
			}
			case XCB_KEY_RELEASE: {
				xcb_key_press_event_t *kp = (void*)ev;
				swl_key_event_t key;
				key.state = 0;
				key.key = kp->detail - 8;
				wl_signal_emit(&x11->input.key, &key);
				break;
			}
			case XCB_CONFIGURE_NOTIFY: {
				xcb_configure_notify_event_t *conf = (void*)ev;
				swl_x11_output_t *out = x11->output;
				out->common.mode.height = conf->height;
				out->common.mode.width = conf->width;

				xcb_free_pixmap(x11->connection, x11->output->pixmaps[0]);
				xcb_free_pixmap(x11->connection, x11->output->pixmaps[1]);

				for(uint32_t i = 0; i < 2; ++i) {
					x11->output->pixmaps[i] = xcb_generate_id(x11->connection);
					
					x11->output->common.renderer->destroy_target(x11->output->common.renderer, x11->output->common.targets[i]);
					swl_gbm_buffer_destroy(x11->output->common.buffer[i]);
					x11->output->common.buffer[i] = swl_gbm_buffer_create(x11->output->dev, x11->output->common.mode.width, x11->output->common.mode.height, GBM_FORMAT_XRGB8888, GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR, 32);
					x11->output->common.targets[i] = x11->output->common.renderer->create_target(x11->output->common.renderer, x11->output->common.buffer[i]);
					xcb_dri3_pixmap_from_buffer(x11->connection, x11->output->pixmaps[i], x11->output->window, x11->output->common.buffer[i]->size, out->common.mode.width, out->common.mode.height, x11->output->common.buffer[i]->pitch, 24, 32, gbm_bo_get_fd(x11->output->common.buffer[i]->bo));
				}
				break;
			}
			case XCB_MOTION_NOTIFY: {
				xcb_motion_notify_event_t *motion = (void*)ev;
				swl_motion_event_t pointer;
				pointer.absx = motion->event_x;
				pointer.absy = motion->event_y;

				pointer.dx = motion->event_x - px;
				pointer.dy = motion->event_y - py;
				
				wl_signal_emit(&x11->input.motion, &pointer);

				px = motion->event_x;
				py = motion->event_y;
				break;
			}
			case XCB_BUTTON_PRESS: {
				xcb_button_press_event_t *xcb_button = (void*)ev;
				swl_button_event_t button = { 0 };
				button.state = 1;
				button.button = swl_xcb_button_to_linux(xcb_button->detail);
				button.time = xcb_button->time;
				wl_signal_emit(&x11->input.button, &button);
				break;
			}
			case XCB_BUTTON_RELEASE: {
				xcb_button_press_event_t *xcb_button = (void*)ev;
				swl_button_event_t button = { 0 };
				button.state = 0;
				button.button = swl_xcb_button_to_linux(xcb_button->detail);
				button.time = xcb_button->time;
				wl_signal_emit(&x11->input.button, &button);
				break;
			}
			case XCB_FOCUS_OUT: {
				wl_signal_emit(&x11->disable, NULL);
				/*UHHH TELL WAYLAND SEAT*/
				break;
			}
			case XCB_FOCUS_IN: {
				wl_signal_emit(&x11->activate, NULL);
				/*UHHH TELL WAYLAND SEAT*/
				break;
			}
			case 0: {
				xcb_generic_error_t *err = (void*)ev;
				break;
			}
			default:
				break;
		}
		free(ev);
		ev = xcb_poll_for_event(x11->connection);
	}
	
	//xcb_copy_area(x11->connection, x11->output->pixmaps[x11->output->common.front_buffer], x11->output->window, x11->output->gc, 0, 0, 0, 0, x11->output->common.buffer[x11->output->common.front_buffer].width, x11->output->common.buffer[x11->output->common.front_buffer].height);	
	
	xcb_flush(x11->connection);


	return 0;
}

int swl_x11_backend_stop(swl_backend_t *backend) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;

	return 0;
}

int swl_x11_backend_switch_vt(swl_backend_t *backend, int vt) {
	return 0;
}

int swl_x11_backend_move_cursor(swl_backend_t *backend, int32_t x, int32_t y) {
	return 0;
}

int swl_x11_backend_start(swl_backend_t *backend) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;
	wl_signal_emit(&x11->new_output, x11->output);
	wl_signal_emit(&x11->new_input, &x11->input);

	wl_signal_emit(&x11->output->common.frame, x11->output);
	xcb_present_pixmap(x11->connection, 
		x11->output->window, 
		x11->output->pixmaps[x11->output->common.front_buffer],
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		XCB_PRESENT_OPTION_NONE,
		0,
		0,
		0,
		0, NULL);


	return 0;
}

void swl_x11_backend_destroy(swl_backend_t *backend) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;
	wl_event_source_remove(x11->event);

	for(uint32_t i = 0; i < 2; ++i) {
		x11->output->common.renderer->destroy_target(x11->output->common.renderer, x11->output->common.targets[i]);
		xcb_free_pixmap(x11->connection, x11->output->pixmaps[i]);
		swl_gbm_buffer_destroy(x11->output->common.buffer[i]);
	}
	gbm_device_destroy(x11->output->dev);
	x11->output->common.renderer->destroy(x11->output->common.renderer);
	xcb_destroy_window(x11->connection, x11->output->window);
	close(x11->drm_fd);

	free(x11->output->common.buffer);
	free(x11->output->common.targets);
	wl_global_destroy(x11->output->common.global);
	free(x11->output);
	xcb_disconnect(x11->connection);
	free(x11);
}

void swl_x11_backend_add_new_output_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;

	wl_signal_add(&x11->new_output, listener);
}

void swl_x11_backend_add_new_input_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;

	wl_signal_add(&x11->new_input, listener);
}

void swl_x11_backend_add_new_activate_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;

	wl_signal_add(&x11->activate, listener);
}

void swl_x11_backend_add_new_disable_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t*)backend;

	wl_signal_add(&x11->disable, listener);
}

swl_renderer_t *swl_x11_backend_get_renderer(swl_backend_t *backend) {
	swl_x11_backend_t *x11 = (swl_x11_backend_t *)backend;
	return x11->output->common.renderer;
}

swl_backend_t *swl_x11_backend_create(struct wl_display *display) {
	swl_x11_backend_t *x11 = calloc(1, sizeof(swl_x11_backend_t));
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int screenp = 0;

	x11->display = display;
	x11->connection = xcb_connect(NULL, &screenp);

	const xcb_query_extension_reply_t *ext = NULL;
	ext = xcb_get_extension_data(x11->connection, &xcb_dri3_id);
	if(!ext | !ext->present) {
		swl_error("X11 DRI3 Extension not supported\n");
		return NULL;
	}

	ext = xcb_get_extension_data(x11->connection, &xcb_present_id);
	if(!ext | !ext->present) {
		swl_error("X11 Present Extension not supported\n");
		return NULL;
	}

	setup = xcb_get_setup(x11->connection);

	iter = xcb_setup_roots_iterator(setup);
	for(; iter.rem; --screenp, xcb_screen_next(&iter)) {
		if(screenp == 0) {
			x11->screen = iter.data;
			break;
		}
	}

	wl_signal_init(&x11->input.key);
	wl_signal_init(&x11->input.button);
	wl_signal_init(&x11->input.motion);
	wl_signal_init(&x11->new_output);
	wl_signal_init(&x11->new_input);
	wl_signal_init(&x11->activate);
	wl_signal_init(&x11->disable);
	x11->event = wl_event_loop_add_fd(loop, xcb_get_file_descriptor(x11->connection), WL_EVENT_READABLE,
			swl_x11_event, x11);

	x11->output = swl_x11_output_create(x11);

	x11->backend.BACKEND_ADD_NEW_OUTPUT_LISTENER = swl_x11_backend_add_new_output_listener;
	x11->backend.BACKEND_ADD_ACTIVATE_LISTENER = swl_x11_backend_add_new_activate_listener;
	x11->backend.BACKEND_ADD_DISABLE_LISTENER = swl_x11_backend_add_new_disable_listener;
	x11->backend.BACKEND_ADD_NEW_INPUT_LISTENER = swl_x11_backend_add_new_input_listener;
	x11->backend.BACKEND_DESTROY = swl_x11_backend_destroy;
	x11->backend.BACKEND_STOP = swl_x11_backend_stop;
	x11->backend.BACKEND_START = swl_x11_backend_start;
	x11->backend.BACKEND_GET_RENDERER = swl_x11_backend_get_renderer;
	x11->backend.BACKEND_SWITCH_VT = swl_x11_backend_switch_vt;
	x11->backend.BACKEND_MOVE_CURSOR = swl_x11_backend_move_cursor;
	return (swl_backend_t*)x11;
}
