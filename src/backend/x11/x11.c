#include "soilleirwl/interfaces/swl_output.h"
#include "soilleirwl/logger.h"
#include "soilleirwl/renderer.h"
#include <gbm.h>
#include <soilleirwl/x11.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <xcb/dri3.h>

int swl_drm_create_fb(int fd, swl_buffer_t *bo, uint32_t width, uint32_t height);


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

#include <fcntl.h>

int swl_x11_create_fb(struct gbm_device *dev, swl_buffer_t *bo, uint32_t width, uint32_t height) {
	bo->render = gbm_device_get_fd(dev);
}

swl_x11_output_t *swl_x11_output_create(swl_x11_backend_t *x11) {
	swl_x11_output_t *out = calloc(1, sizeof(swl_x11_output_t));

	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_BIT_GRAVITY;
	uint32_t values[3];

	values[0] = 0x000000;
	values[1] = XCB_GRAVITY_STATIC;
	values[2] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

	out->window = xcb_generate_id(x11->connection);
	xcb_create_window(x11->connection, 24, out->window, x11->screen->root, 
			0, 0, 640, 480, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT, x11->screen->root_visual,
			mask, values);

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
	swl_debug("%p\n", gbm_create_device);
	x11->output->dev = gbm_create_device(fds[0]);
	swl_debug("GBM Dev: %p\n", x11->output->dev);

	x11->output->bos[0] = gbm_bo_create(x11->output->dev, 1920, 1080, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
	x11->output->bos[1] = gbm_bo_create(x11->output->dev, 1920, 1080, GBM_FORMAT_XRGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
	swl_debug("B1 %p B2 %p\n", x11->output->bos[0], x11->output->bos[1]);
	for(uint8_t i = 0; i < 2; i++) {
		x11->output->common.buffer[i].height = gbm_bo_get_height(x11->output->bos[i]);
		x11->output->common.buffer[i].width = gbm_bo_get_width(x11->output->bos[i]);
		x11->output->common.buffer[i].pitch = gbm_bo_get_stride(x11->output->bos[i]);
		x11->output->common.buffer[i].handle = gbm_bo_get_handle(x11->output->bos[i]).u32;
		x11->output->common.buffer[i].offset = 0;
		x11->output->common.buffer[i].size = x11->output->common.buffer[i].height * x11->output->common.buffer[i].pitch;
		x11->output->common.buffer[i].data = gbm_bo_map(x11->output->bos[i], 0, 0,
				x11->output->common.buffer[i].width, x11->output->common.buffer[i].height,
				0, &x11->output->common.buffer[i].pitch, NULL);
		x11->output->common.buffer[i].render = fds[0];

	}

	wl_signal_init(&out->common.destroy);
	wl_signal_init(&out->common.frame);

	out->common.mode.width = 1920;
	out->common.mode.height = 1080;
	out->common.renderer = swl_egl_renderer_create_by_fd(fds[0]);

	out->gc = xcb_generate_id(x11->connection);
	xcb_create_gc(x11->connection, out->gc, out->window, 0, NULL);

	wl_global_create(x11->display, &wl_output_interface, SWL_OUTPUT_VERSION, out, swl_output_bind);
	
	for(uint32_t i = 0; i < 2; i++) {
		x11->output->pixmaps[i] = xcb_generate_id(x11->connection);
		xcb_dri3_pixmap_from_buffer(x11->connection, x11->output->pixmaps[i], x11->output->window, x11->output->common.buffer[i].size, 1920, 1080, x11->output->common.buffer[i].pitch, 24, 32, gbm_bo_get_fd(x11->output->bos[i]));
	}
	return out;
}

int swl_x11_event(int fd, uint32_t mask, void *data) {
	swl_x11_backend_t *x11 = data;

	xcb_generic_event_t *ev = xcb_poll_for_event(x11->connection);
	while(ev) {
		switch(ev->response_type) {
			case 14: {
				return 0;
				break;
			}
			case 0: {
				xcb_generic_error_t *err = ev;
				printf("%d.%d %d\n", err->major_code, err->minor_code, err->error_code);
				break;
			}
			default:
				printf("%d\n", ev->response_type);
		}
		free(ev);
		ev = xcb_poll_for_event(x11->connection);
	}
		printf("Copying\n");
		wl_signal_emit(&x11->output->common.frame, x11->output);
		xcb_copy_area(x11->connection, x11->output->pixmaps[x11->output->common.front_buffer], x11->output->window, x11->output->gc, 0, 0, 0, 0, x11->output->common.buffer[x11->output->common.front_buffer].width, x11->output->common.buffer[x11->output->common.front_buffer].height);	
		
		xcb_flush(x11->connection);


	return 0;
}

swl_renderer_t *swl_x11_backend_get_renderer(swl_x11_backend_t *backend) {
	return backend->output->common.renderer;
}


int swl_x11_backend_start(swl_x11_backend_t *x11) {
	wl_signal_emit(&x11->new_output, x11->output);
	
	return 0;	
}

swl_x11_backend_t *swl_x11_backend_create(struct wl_display *display) {
	swl_x11_backend_t *x11 = calloc(1, sizeof(swl_x11_backend_t));
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int screenp = 0;

	x11->display = display;
	x11->connection = xcb_connect(NULL, &screenp);

	setup = xcb_get_setup(x11->connection);
	
	iter = xcb_setup_roots_iterator(setup);
	for(; iter.rem; --screenp, xcb_screen_next(&iter)) {
		if(screenp == 0) {
			x11->screen = iter.data;
			break;
		}
	}

	wl_signal_init(&x11->new_input);
	wl_signal_init(&x11->new_output);
	swl_debug("TEST X11 \n");
	wl_event_loop_add_fd(loop, xcb_get_file_descriptor(x11->connection), WL_EVENT_READABLE,
			swl_x11_event, x11);

	x11->output = swl_x11_output_create(x11);

	x11->get_backend_renderer = swl_x11_backend_get_renderer;
	return x11;
}
