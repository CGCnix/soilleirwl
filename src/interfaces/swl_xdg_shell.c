#include "soilleirwl/interfaces/swl_surface.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <private/xdg-shell-server.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <soilleirwl/interfaces/swl_xdg_shell.h>

#include <soilleirwl/utils.h>
#include <wayland-util.h>

static void swl_xdg_popup_destroy(struct wl_client *client, struct wl_resource *popup) {
	wl_resource_destroy(popup);
}

static void swl_xdg_popup_resource_destroy(struct wl_resource *popup_resource) {
	swl_xdg_popup_t *popup = wl_resource_get_user_data(popup_resource);

	wl_signal_emit_mutable(&popup->destroy, NULL);
	free(popup);
}

static void swl_xdg_popup_grab(struct wl_client *client, struct wl_resource *popup, struct wl_resource *seat, uint32_t serial) {

}

static void swl_xdg_popup_reposition(struct wl_client *client, struct wl_resource *popup, struct wl_resource *positioner, uint32_t token) {
	swl_xdg_popup_t *swl_popup = wl_resource_get_user_data(popup);
	swl_xdg_positioner_t *pos = wl_resource_get_user_data(positioner);
	
	swl_popup->anchor = pos->anchor;
	swl_popup->size = pos->size;
	swl_popup->gravity = pos->gravity;
	swl_popup->repos = 1;
	swl_popup->token = token;
}

static struct xdg_popup_interface swl_xdg_popup_impl = {
	.destroy = swl_xdg_popup_destroy,
	.grab = swl_xdg_popup_grab,
	.reposition = swl_xdg_popup_reposition,
};

static void swl_xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *toplevel) {
	wl_resource_destroy(toplevel);
}

static void swl_xdg_toplevel_handle_resource_destroy(struct wl_resource *toplevel) {
	swl_xdg_toplevel_t *swl_xdg_toplevel;

	swl_xdg_toplevel = wl_resource_get_user_data(toplevel);

	wl_signal_emit_mutable(&swl_xdg_toplevel->destroy, swl_xdg_toplevel);

	if(swl_xdg_toplevel->title) free((char*)swl_xdg_toplevel->title);
	free(swl_xdg_toplevel);
}

static void swl_xdg_toplevel_set_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

static void swl_xdg_toplevel_unset_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

static void swl_xdg_toplevel_set_minimized(struct wl_client *client, struct wl_resource *toplevel) {

}

static void swl_xdg_toplevel_set_fullscreen(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *output) {

}


static void swl_xdg_toplevel_set_parent(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *parent) {
	swl_xdg_toplevel_t *swl_xdg_toplevel = wl_resource_get_user_data(toplevel);
	swl_xdg_surface_t *swl_xdg_surface = wl_resource_get_user_data(swl_xdg_toplevel->xdg_surface);
	struct wl_display *display = wl_client_get_display(client);
	
	if(swl_xdg_surface->idle_configure == NULL) {
		swl_xdg_surface->idle_configure = wl_event_loop_add_idle(wl_display_get_event_loop(display),
		swl_xdg_surface_send_configure, swl_xdg_surface);
	}
}

static void swl_xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *toplevel) {

}

static void swl_xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *toplevel, 
		const char *title) {
	swl_xdg_toplevel_t *top = wl_resource_get_user_data(toplevel);
	top->title = strdup(title);
}

static void swl_xdg_toplevel_set_appid(struct wl_client *client, struct wl_resource *toplevel, 
		const char *appid) {

}

static void swl_xdg_toplevel_set_max_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

static void swl_xdg_toplevel_set_min_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

static void swl_xdg_toplevel_show_window_menu(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {

}

static void swl_xdg_toplevel_move(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial) {

}

static void swl_xdg_toplevel_resize(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial, uint32_t edges) {
}

static const struct xdg_toplevel_interface xdg_toplevel_impl = {
	.resize = swl_xdg_toplevel_resize,
	.move = swl_xdg_toplevel_move,
	.destroy = swl_xdg_toplevel_destroy,
	.show_window_menu = swl_xdg_toplevel_show_window_menu,
	.set_max_size = swl_xdg_toplevel_set_max_size,
	.set_min_size = swl_xdg_toplevel_set_min_size,
	.set_app_id = swl_xdg_toplevel_set_appid,
	.set_title = swl_xdg_toplevel_set_title,
	.set_parent = swl_xdg_toplevel_set_parent,
	.unset_fullscreen = swl_xdg_toplevel_unset_fullscreen,
	.set_fullscreen = swl_xdg_toplevel_set_fullscreen,
	.set_minimized = swl_xdg_toplevel_set_minimized,
	.set_maximized = swl_xdg_toplevel_set_maximized,
	.unset_maximized = swl_xdg_toplevel_unset_maximized,
};

static void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void xdg_surface_handle_destroy(struct wl_resource *resource) {
	swl_xdg_surface_t *surface = wl_resource_get_user_data(resource);
	wl_signal_emit_mutable(&surface->destroy, surface);
	free(surface);
}

static void xdg_surface_toplevel_configure(struct wl_client *client, swl_xdg_surface_t *surface, struct wl_resource *resource) {
	swl_surface_t *swl_surface = wl_resource_get_user_data(surface->wl_surface_res);
	struct wl_array states;
	wl_array_init(&states);

	xdg_toplevel_send_configure(resource, swl_surface->width, swl_surface->height, &states);
}

static void xdg_surface_popup_configure(struct wl_client *client, swl_xdg_surface_t *surface, struct wl_resource *resource) {
	swl_xdg_popup_t *popup = wl_resource_get_user_data(resource);
	swl_surface_t *base_surface = wl_resource_get_user_data(surface->wl_surface_res);
	swl_xdg_surface_t *parent = wl_resource_get_user_data(popup->parent);
	swl_surface_t *parent_surface = wl_resource_get_user_data(parent->wl_surface_res);

	base_surface->height = popup->size.height;
	base_surface->width = popup->size.width;
	base_surface->position.x = popup->anchor.x + parent_surface->position.x;
	base_surface->position.y = popup->anchor.y + parent_surface->position.y;

	if(popup->repos) {
		xdg_popup_send_repositioned(resource, popup->token);
		popup->repos = 0;
	}
	xdg_popup_send_configure(resource, popup->anchor.x, popup->anchor.y, popup->size.width, popup->size.height);
}


void swl_xdg_surface_send_configure(void *data) {
	swl_xdg_surface_t *surface = data;

	surface->idle_configure = NULL;

	surface->role->configure(wl_resource_get_client(surface->role_resource), surface, surface->role_resource);
	xdg_surface_send_configure(surface->resource, wl_display_next_serial(swl_get_display_from_resource(surface->resource)));
}

static swl_xdg_surface_role_t swl_xdg_surface_toplevel_role = {
	.configure = xdg_surface_toplevel_configure,
};

static swl_xdg_surface_role_t swl_xdg_surface_popup_role = {
	.configure = xdg_surface_popup_configure,
};

static void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *xdg_surface,
	uint32_t id, struct wl_resource *parent, struct wl_resource *xdg_positioner) {
	swl_xdg_popup_t *popup = calloc(1, sizeof(swl_xdg_popup_t));
	swl_xdg_surface_t *surface = wl_resource_get_user_data(xdg_surface);
	swl_xdg_positioner_t *positioner = wl_resource_get_user_data(xdg_positioner);

	popup->parent = parent;
	popup->xdg_surface = xdg_surface;

	popup->gravity = positioner->gravity;
	popup->anchor = positioner->anchor;
	popup->offset = positioner->offset;
	popup->size = positioner->size;

	popup->resource = wl_resource_create(client, &xdg_popup_interface, wl_resource_get_version(xdg_surface), id);
	wl_resource_set_implementation(popup->resource, &swl_xdg_popup_impl, popup, swl_xdg_popup_resource_destroy);
	surface->role_resource = popup->resource;
	surface->role = &swl_xdg_surface_popup_role;

	wl_signal_init(&popup->destroy);
	wl_signal_emit(&surface->new_popup, popup);
}

static void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface,
		uint32_t id) {
	swl_xdg_toplevel_t *swl_xdg_toplevel = calloc(1, sizeof(swl_xdg_toplevel_t));
	swl_xdg_surface_t *swl_xdg_surface = wl_resource_get_user_data(xdg_surface);
	swl_surface_t *swl_surface = wl_resource_get_user_data(swl_xdg_surface->wl_surface_res);
	struct wl_array caps;
	wl_array_init(&caps);

	swl_xdg_toplevel->xdg_surface = xdg_surface;

	swl_xdg_toplevel->resource = wl_resource_create(client, &xdg_toplevel_interface, wl_resource_get_version(xdg_surface), id);
	wl_resource_set_implementation(swl_xdg_toplevel->resource, &xdg_toplevel_impl, swl_xdg_toplevel, swl_xdg_toplevel_handle_resource_destroy);

	swl_xdg_surface->role = &swl_xdg_surface_toplevel_role;
	swl_xdg_surface->role_resource = swl_xdg_toplevel->resource;
	swl_surface->width = 640;
	swl_surface->height = 480;
	swl_xdg_surface->geometry.width = swl_surface->width;
	swl_xdg_surface->geometry.height = swl_surface->height;
	swl_xdg_surface->geometry.x = swl_surface->position.x;
	swl_xdg_surface->geometry.y = swl_surface->position.y;

	if(wl_resource_get_version(swl_xdg_toplevel->resource) >= XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
		xdg_toplevel_send_wm_capabilities(swl_xdg_toplevel->resource, &caps);
	}
	wl_signal_init(&swl_xdg_toplevel->destroy);
	wl_signal_emit(&swl_xdg_surface->new_toplevel, swl_xdg_toplevel);
}

static void xdg_surface_set_geometry(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {
}

static void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource,
		uint32_t serial) {
	swl_xdg_surface_t *surface = wl_resource_get_user_data(resource);
	/*TODO check maybe i.e. if a client is running behind*/
}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy = xdg_surface_destroy,
	.set_window_geometry = xdg_surface_set_geometry,
	.get_popup = xdg_surface_get_popup,
	.get_toplevel = xdg_surface_get_toplevel,
	.ack_configure = xdg_surface_ack_configure,
};

static void swl_xdg_surface_precommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {
	swl_xdg_surface_t *xdg_surface = wl_resource_get_user_data(resource);


	if(!surface->configured && !surface->buffer.buffer) {
		/*Initial first commit*/
		surface->configured = 1;
		if(xdg_surface->idle_configure == NULL) {
			xdg_surface->idle_configure = wl_event_loop_add_idle(wl_display_get_event_loop(wl_client_get_display(client)),
				swl_xdg_surface_send_configure, xdg_surface);
		} 
	}
}

static void swl_xdg_surface_postcommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {

}

static swl_surface_role_t swl_xdg_surface_role = {	
	.postcommit = swl_xdg_surface_postcommit,
	.precommit = swl_xdg_surface_precommit,
};

static void swl_xdg_positioner_destroy(struct wl_client *client, struct wl_resource *positioner) {
	wl_resource_destroy(positioner);
}

static void swl_xdg_positioner_resource_destroy(struct wl_resource *positioner) {
	free(wl_resource_get_user_data(positioner));
}

static void swl_xdg_positioner_set_anchor(struct wl_client *client, struct wl_resource *positioner, uint32_t anchor) {
	swl_xdg_positioner_t *xdg_positioner = wl_resource_get_user_data(positioner);

	xdg_positioner->anchor.type = anchor;
}

static void swl_xdg_positioner_set_size(struct wl_client *client, struct wl_resource *positioner, int32_t width, int32_t height) {
	swl_xdg_positioner_t *xdg_positioner = wl_resource_get_user_data(positioner);

	xdg_positioner->size.width = width;
	xdg_positioner->size.height = height;
}

static void swl_xdg_positioner_set_anchor_rect(struct wl_client *client, struct wl_resource *positioner, int32_t x, int32_t y, int32_t w, int32_t h) {
	swl_xdg_positioner_t *xdg_positioner = wl_resource_get_user_data(positioner);

	xdg_positioner->anchor.width = w;
	xdg_positioner->anchor.height = h;
	xdg_positioner->anchor.x = x;
	xdg_positioner->anchor.y = y;
}

static void swl_xdg_positioner_set_gravity(struct wl_client *client, struct wl_resource *positioner, uint32_t gravity) {
	swl_xdg_positioner_t *xdg_positioner = wl_resource_get_user_data(positioner);

	xdg_positioner->gravity = gravity;
}

static void swl_xdg_positioner_set_constraint_adjustment(struct wl_client *client, struct wl_resource *positioner, uint32_t constraint_adj) {

}

static void swl_xdg_positioner_set_offset(struct wl_client *client, struct wl_resource *positioner, int32_t x, int32_t y) {

}

static void swl_xdg_positioner_set_reactive(struct wl_client *client, struct wl_resource *positioner) {

}

static void swl_xdg_positioner_set_parent_size(struct wl_client *client, struct wl_resource *positioner, int32_t width, int32_t height) {

}

static void swl_xdg_positioner_set_parent_configure(struct wl_client *client, struct wl_resource *positioner, uint32_t serial) {

}


static struct xdg_positioner_interface swl_xdg_positioner_impl = {
	.destroy = swl_xdg_positioner_destroy,
	.set_anchor_rect = swl_xdg_positioner_set_anchor_rect,
	.set_anchor = swl_xdg_positioner_set_anchor,
	.set_size = swl_xdg_positioner_set_size,
	.set_parent_size = swl_xdg_positioner_set_parent_size,
	.set_parent_configure = swl_xdg_positioner_set_parent_configure,
	.set_constraint_adjustment = swl_xdg_positioner_set_constraint_adjustment,
	.set_gravity = swl_xdg_positioner_set_gravity,
	.set_offset = swl_xdg_positioner_set_offset,
	.set_reactive = swl_xdg_positioner_set_reactive,
};

static void swl_xdg_wm_base_pong(struct wl_client *client, struct wl_resource *xdg_base, uint32_t serial) {
	/*TODO Maybe Verify Serial*/
}

static void swl_xdg_wm_base_get_xdg_surface(struct wl_client *client, struct wl_resource *xdg_base, uint32_t id, struct wl_resource *wl_surface) {
	swl_surface_t *swl_surface;
	swl_xdg_wm_base_t *swl_xdg_base = wl_resource_get_user_data(xdg_base);
	swl_xdg_surface_t *swl_xdg_surface = calloc(1, sizeof(swl_xdg_surface_t));

	swl_surface = wl_resource_get_user_data(wl_surface);
	swl_surface->role = &swl_xdg_surface_role;
	
	swl_xdg_surface->wl_surface_res = swl_surface->resource;
	swl_xdg_surface->resource = wl_resource_create(client, &xdg_surface_interface, wl_resource_get_version(xdg_base), id);
	swl_surface->role_resource = swl_xdg_surface->resource;
	wl_resource_set_implementation(swl_xdg_surface->resource, &xdg_surface_impl, swl_xdg_surface, xdg_surface_handle_destroy);

	wl_signal_init(&swl_xdg_surface->destroy);
	wl_signal_init(&swl_xdg_surface->new_toplevel);
	wl_signal_init(&swl_xdg_surface->new_popup);
	wl_signal_emit(&swl_xdg_base->new_surface, swl_xdg_surface);
}

static void swl_xdg_wm_base_create_positioner(struct wl_client *client, struct wl_resource *xdg_base, uint32_t id) {
	swl_xdg_positioner_t *positioner = calloc(1, sizeof(swl_xdg_positioner_t));
	swl_xdg_wm_base_t *swl_xdg_base = wl_resource_get_user_data(xdg_base);

	positioner->resource = wl_resource_create(client, &xdg_positioner_interface, wl_resource_get_version(xdg_base), id);
	wl_resource_set_implementation(positioner->resource, &swl_xdg_positioner_impl, positioner, swl_xdg_positioner_resource_destroy);
}

static void swl_xdg_wm_base_handle_destroy(struct wl_client *client, struct wl_resource *xdg_base) {
	wl_resource_destroy(xdg_base);
}

static void swl_xdg_wm_base_handle_resource_destory(struct wl_resource *xdg_base) {

}


const static struct xdg_wm_base_interface swl_wm_base_impl = {
	.pong = swl_xdg_wm_base_pong,
	.get_xdg_surface = swl_xdg_wm_base_get_xdg_surface,
	.create_positioner = swl_xdg_wm_base_create_positioner,
	.destroy = swl_xdg_wm_base_handle_destroy,
};

static void swl_xdg_wm_base_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *xdg_base_res;
	swl_xdg_wm_base_t *xdg_base;
	xdg_base_res = wl_resource_create(client, &xdg_wm_base_interface, version, id);
	if(!xdg_base_res) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(xdg_base_res, &swl_wm_base_impl, data, swl_xdg_wm_base_handle_resource_destory);
	xdg_wm_base_send_ping(xdg_base_res, wl_display_next_serial(wl_client_get_display(client)));
}

void swl_xdg_wm_base_destroy(swl_xdg_wm_base_t *xdg_base) {
	wl_global_destroy(xdg_base->global);
	free(xdg_base);
}

swl_xdg_wm_base_t *swl_xdg_wm_base_create(struct wl_display *display, void *data) {
	swl_xdg_wm_base_t *wm_base = calloc(1, sizeof(swl_xdg_wm_base_t));

	wl_signal_init(&wm_base->new_surface);
	wl_signal_init(&wm_base->new_positioner);
	wm_base->global = wl_global_create(display, &xdg_wm_base_interface, SWL_XDG_WM_BASE_VERSION, wm_base, swl_xdg_wm_base_bind);
	return wm_base;
}
