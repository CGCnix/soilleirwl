#pragma once

#include <gbm.h>
#include <soilleirwl/interfaces/swl_output.h>

#include <stdint.h>
#include <wayland-server-core.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <wayland-server.h>

typedef struct swl_x11_output {
	swl_output_t common;
	
	xcb_window_t window;
	xcb_gcontext_t gc;
	xcb_pixmap_t pixmaps[2];
	struct gbm_device *dev;
	struct gbm_bo *bos[2];
} swl_x11_output_t;

typedef struct swl_x11_backend {
	struct wl_display *display;
	xcb_connection_t *connection;
	xcb_screen_t *screen;

	swl_x11_output_t *output;

	struct wl_signal new_output;
	struct wl_signal key;
	struct wl_signal pointer;

	/*Tell others*/
	struct wl_signal activate;
	struct wl_signal disable;

	struct wl_event_source *event;

	swl_renderer_t *(*get_backend_renderer)(struct swl_x11_backend *drm);
} swl_x11_backend_t;

swl_x11_backend_t *swl_x11_backend_create(struct wl_display *display);
int swl_x11_backend_start(swl_x11_backend_t *x11);
void swl_x11_backend_destroy(swl_x11_backend_t *x11);
