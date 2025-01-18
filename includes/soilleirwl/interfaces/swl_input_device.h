#pragma once

#include <stdint.h>
#include <wayland-server.h>

typedef struct swl_input_dev {
	/*TODO this is kinda messy as not all devices will 
	 * always have all these features.
	 * TODO Look into spliting this up in a more logical fashion
	 * i.e. EXPOSE devices as having only KEY events or ONLY 
	 * Pointer events.
	 */
	struct wl_signal key;
	struct wl_signal motion;
	struct wl_signal button;
} swl_input_dev_t;

typedef struct swl_input_key_event {
	const swl_input_dev_t *device;
	uint32_t key;
	uint32_t time;
	uint32_t state;
} swl_key_event_t;

typedef struct swl_motion_event {
	uint32_t dx;
	uint32_t dy;
	uint32_t absx;
	uint32_t absy;
	uint32_t time;
} swl_motion_event_t;

typedef struct swl_button_event {
	uint32_t button;
	uint32_t state;
	uint32_t time;
} swl_button_event_t;
