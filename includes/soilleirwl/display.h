#pragma once

#include <soilleirwl/renderer.h>
#include <wayland-server-core.h>
#include <soilleirwl/session.h>

typedef struct swl_display_backend {
	struct wl_signal new_output;
	int (*get_drm_fd)(struct swl_display_backend *drm);
	swl_renderer_t *(*get_backend_renderer)(struct swl_display_backend *drm);
} swl_display_backend_t;



int swl_drm_backend_stop(swl_display_backend_t *display);
int swl_drm_backend_start(swl_display_backend_t *display);
swl_display_backend_t *swl_drm_create_backend(struct wl_display *display, swl_session_backend_t *session, const char *drm_device);
void swl_drm_backend_destroy(swl_display_backend_t *display, swl_session_backend_t *session);
