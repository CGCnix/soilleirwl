#pragma once

#include <soilleirwl/backend/session.h>
#include <soilleirwl/backend/hotplug.h>

#include <wayland-server.h>

typedef struct swl_input_backend swl_input_backend_t;

struct swl_input_backend {
	struct wl_signal new_input;

	int (*SWL_INPUT_BACKEND_START)(swl_input_backend_t *input);
	int (*SWL_INPUT_BACKEND_STOP)(swl_input_backend_t *input);
	void (*SWL_INPUT_BACKEND_DESTROY)(swl_input_backend_t *input);
};


swl_input_backend_t *swl_libinput_backend_create(struct wl_display *display,
		swl_session_backend_t *session, swl_hotplug_backend_t *dev_man);
