#pragma once

#include <wayland-server-core.h>
#include <soilleirwl/session.h>
#include <soilleirwl/dev_man.h>

/*Input Device Types*/
enum {
	SWL_DEVICE_KEYBOARD,
	SWL_DEVICE_MOUSE,
	SWL_DEVICE_TOUCH,
};

typedef struct {
	struct wl_signal new_input;
} swl_input_backend_t;


swl_input_backend_t *swl_libinput_backend_create(struct wl_display *display,
		swl_session_backend_t *session, swl_dev_man_backend_t *dev_man);

