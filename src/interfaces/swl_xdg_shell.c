#include <stdint.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>


typedef struct swl_xdg_wm_base {
	struct wl_global *global;
	/*This was the last serial send in ping*/
	uint32_t serial;
} swl_xdg_wm_base_t;

static xdg_wm_base

static void swl_xdg_wm_base_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {

}

swl_xdg_wm_base_t *swl_xdg_wm_base_create(struct wl_display *display) {

}
