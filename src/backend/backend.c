#include <soilleirwl/backend/backend.h>
#include <soilleirwl/backend/tty.h>
#include <soilleirwl/backend/xcb.h>
#include <stdlib.h>

swl_backend_t *swl_backend_create_by_env(struct wl_display *display) {
	if(getenv("DISPLAY")) {
		return swl_x11_backend_create(display);
	}

	return swl_tty_backend_create(display);
}
