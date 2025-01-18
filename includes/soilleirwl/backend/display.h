#pragma once

#include "soilleirwl/renderer.h"
#include <soilleirwl/backend/session.h>
#include <wayland-server-core.h>

typedef struct swl_display_backend swl_display_backend_t;

struct swl_display_backend {
	struct wl_signal new_output;

	int (*SWL_DISPLAY_BACKEND_STOP)(swl_display_backend_t *display);
	int (*SWL_DISPLAY_BACKEND_START)(swl_display_backend_t *display);
	swl_renderer_t *(*SWL_DISPLAY_BACKEND_GET_RENDERER)(swl_display_backend_t *display);
	void (*SWL_DISPLAY_BACKEND_DESTROY)(swl_display_backend_t *display, swl_session_backend_t *session);
};

swl_display_backend_t *swl_drm_create_backend(struct wl_display *display, swl_session_backend_t *session, const char *drm_device);
