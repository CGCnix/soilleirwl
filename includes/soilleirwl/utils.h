#pragma once

#include <wayland-server-core.h>
#include <assert.h>

static inline struct wl_display *swl_get_display_from_resource(struct wl_resource *resource) {
	assert(resource);
	return wl_client_get_display(wl_resource_get_client(resource));
}
