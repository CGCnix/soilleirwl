#pragma once

#include <wayland-server-core.h>
typedef struct swl_hotplug_backend swl_hotplug_backend_t;

typedef int (*SWL_HOTPLUG_BACKEND_START)(swl_hotplug_backend_t *hotplug);
typedef int (*SWL_HOTPLUG_BACKEND_STOP)(swl_hotplug_backend_t *hotplug);
typedef void (*SWL_HOTPLUG_BACKEND_DESTROY)(swl_hotplug_backend_t *hotplug);

struct swl_hotplug_backend {
	SWL_HOTPLUG_BACKEND_STOP stop;
	SWL_HOTPLUG_BACKEND_START start;
	SWL_HOTPLUG_BACKEND_DESTROY destroy;

	struct wl_signal new_input;
};

swl_hotplug_backend_t *swl_libudev_backend_create(struct wl_display *display);
