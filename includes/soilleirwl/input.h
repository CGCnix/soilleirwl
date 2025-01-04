#pragma once

#include <stdint.h>
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
	uint32_t key;
	uint32_t state;
} swl_key_event_t;

typedef struct {
	int32_t dx;
	int32_t dy;
} swl_pointer_event_t;

typedef struct {
	struct wl_signal new_input;
	struct wl_signal key;
	struct wl_signal pointer;
} swl_input_backend_t;


swl_input_backend_t *swl_libinput_backend_create(struct wl_display *display,
		swl_session_backend_t *session, swl_dev_man_backend_t *dev_man);
void swl_libinput_backend_destroy(swl_input_backend_t *input);
	
