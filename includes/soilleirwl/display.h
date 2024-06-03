#pragma once

#include <wayland-server-core.h>
#include <soilleirwl/session.h>

typedef struct swl_display_backend {
	struct wl_signal new_output;
} swl_display_backend_t;



int swl_drm_backend_stop(swl_display_backend_t *display);
int swl_drm_backend_start(swl_display_backend_t *display);
swl_display_backend_t *swl_drm_create_backend(struct wl_display *display, swl_session_backend_t *session);
	
