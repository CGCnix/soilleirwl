#pragma once

/* Device management should handle
 * - Device hotplugs
 * - Device that exist at boot
 */

#include <wayland-server-core.h>

typedef struct {
	struct wl_signal new_input;
}	swl_dev_man_backend_t;


int swl_udev_backend_start(void *udev);
void swl_udev_backend_destroy(swl_dev_man_backend_t *dev_man);
swl_dev_man_backend_t *swl_udev_backend_create(struct wl_display *display);	
