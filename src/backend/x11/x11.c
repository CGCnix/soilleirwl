#include "soilleirwl/input.h"
#include "soilleirwl/interfaces/swl_output.h"
#include "soilleirwl/logger.h"
#include "soilleirwl/renderer.h"
#include <gbm.h>
#include <soilleirwl/x11.h>

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

	int drm_fd = fds[0];
	swl_debug("X11 FD %d\n", fds[0]);
	int flags = fcntl(drm_fd, F_GETFD);
	fcntl(drm_fd, F_SETFD, flags | FD_CLOEXEC);

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

	swl_x11_create_fb(x11->output->dev, &x11->output->bos[0], &x11->output->common.buffer[0], out->common.mode.width, out->common.mode.height);
	swl_x11_create_fb(x11->output->dev, &x11->output->bos[1], &x11->output->common.buffer[1], out->common.mode.width, out->common.mode.height);

	for(uint32_t i = 0; i < 2; i++) {
		x11->output->pixmaps[i] = xcb_generate_id(x11->connection);
		xcb_dri3_pixmap_from_buffer(x11->connection, x11->output->pixmaps[i], x11->output->window, x11->output->common.buffer[i].size, out->common.mode.width, out->common.mode.height, x11->output->common.buffer[i].pitch, 24, 32, gbm_bo_get_fd(x11->output->bos[i]));
	}

	wl_signal_init(&out->common.destroy);
	wl_signal_init(&out->common.frame);

	wl_global_create(x11->display, &wl_output_interface, SWL_OUTPUT_VERSION, out, swl_output_bind);
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
				printf("EXPOSE\n");
				break;
			}
			case XCB_GRAPHICS_EXPOSURE: {
				printf("Graphics Exposure\n");
				break;
			}
			case XCB_NO_EXPOSURE: {
				printf("No Expose\n");
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
				wl_signal_emit(&x11->key, &key);
				break;
			}
			case XCB_KEY_RELEASE: {
				xcb_key_press_event_t *kp = (void*)ev;
				swl_key_event_t key;
				key.state = 0;
				key.key = kp->detail - 8;
				wl_signal_emit(&x11->key, &key);
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
					swl_x11_destroy_fb(&x11->output->bos[i], &x11->output->common.buffer[i]);
					swl_x11_create_fb(x11->output->dev, &x11->output->bos[i], &x11->output->common.buffer[i], x11->output->common.mode.width, x11->output->common.mode.height);
					xcb_dri3_pixmap_from_buffer(x11->connection, x11->output->pixmaps[i], x11->output->window, x11->output->common.buffer[i].size, out->common.mode.width, out->common.mode.height, x11->output->common.buffer[i].pitch, 24, 32, gbm_bo_get_fd(x11->output->bos[i]));
				}
				break;
			}
			case XCB_MOTION_NOTIFY: {
				xcb_motion_notify_event_t *motion = (void*)ev;
				swl_pointer_event_t pointer;
				pointer.absx = motion->event_x;
				pointer.absy = motion->event_y;

				pointer.dx = motion->event_x - px;
				pointer.dy = motion->event_y - py;
				
				wl_signal_emit(&x11->pointer, &pointer);

				px = motion->event_x;
				py = motion->event_y;
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
				printf("%X11 Error d.%d %d\n", err->major_code, err->minor_code, err->error_code);
				break;
			}
			default:
				printf("%d %d\n", ev->response_type, XCB_PRESENT_COMPLETE_NOTIFY);	
				break;
		}
		free(ev);
		ev = xcb_poll_for_event(x11->connection);
	}
	
	//xcb_copy_area(x11->connection, x11->output->pixmaps[x11->output->common.front_buffer], x11->output->window, x11->output->gc, 0, 0, 0, 0, x11->output->common.buffer[x11->output->common.front_buffer].width, x11->output->common.buffer[x11->output->common.front_buffer].height);	
	
	xcb_flush(x11->connection);


	return 0;
}

swl_renderer_t *swl_x11_backend_get_renderer(swl_x11_backend_t *backend) {
	return backend->output->common.renderer;
}


int swl_x11_backend_start(swl_x11_backend_t *x11) {
	wl_signal_emit(&x11->new_output, x11->output);


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

void swl_x11_backend_destroy(swl_x11_backend_t *x11) {
	wl_event_source_remove(x11->event);

	for(uint32_t i = 0; i < 2; ++i) {
		xcb_free_pixmap(x11->connection, x11->output->pixmaps[i]);
		swl_x11_destroy_fb(&x11->output->bos[i], NULL);
	}
	gbm_device_destroy(x11->output->dev);
	x11->output->common.renderer->destroy(x11->output->common.renderer);
	xcb_destroy_window(x11->connection, x11->output->window);
	free(x11->output);
	xcb_disconnect(x11->connection);
	free(x11);
}

swl_x11_backend_t *swl_x11_backend_create(struct wl_display *display) {
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

	wl_signal_init(&x11->key);
	wl_signal_init(&x11->pointer);
	wl_signal_init(&x11->new_output);
	wl_signal_init(&x11->activate);
	wl_signal_init(&x11->disable);
	x11->event = wl_event_loop_add_fd(loop, xcb_get_file_descriptor(x11->connection), WL_EVENT_READABLE,
			swl_x11_event, x11);

	x11->output = swl_x11_output_create(x11);

	x11->get_backend_renderer = swl_x11_backend_get_renderer;
	return x11;
}
