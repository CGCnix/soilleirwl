#include "../src/xdg-shell-server.h"
#include "../src/swl-screenshot-server.h"
#include "soilleirwl/display.h"
#include "soilleirwl/renderer.h"
#include <soilleirwl/session.h>
#include <soilleirwl/input.h>
#include <soilleirwl/dev_man.h>
#include <soilleirwl/logger.h>


#include <soilleirwl/interfaces/swl_output.h>

#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <xkbcommon/xkbcommon.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/un.h>
#include <sys/socket.h>


void malloc_debug_info();

#define MODIFER_CTRL 4
#define MODIFER_ALT 8

enum {
	SERVER_ADD_KEYBIND,
	SERVER_SET_BACKGRN,
};

typedef struct{
	uint32_t opcode;
	uint32_t len;
} server_ipc_msg_t;

typedef struct {
	int fd;
	struct wl_event_source *source;
} server_ipc_sock;

typedef struct {
	struct wl_resource *surface;
	struct wl_resource *buffer;
	struct wl_resource *shell_surface;
	struct wl_resource *callback;
	/*Old texture;
	 *swl_output_texture_t texture;
	 */

	swl_texture_t *texture;
	int32_t x, y;
} swl_surface_t;

typedef struct swl_client {
	struct wl_list surfaces;
	struct wl_resource *keyboard;

	struct wl_client *client;

	struct wl_listener destroy;

	struct wl_list link;
}	swl_client_t;

typedef struct swl_seat {
	const char *seat_name;
	uint32_t caps;

	struct wl_listener key;

	struct xkb_context *xkb;
	struct xkb_keymap *map;
	struct xkb_state *state;
} swl_seat_t;


typedef struct swl_xdg_toplevel swl_xdg_toplevel_t;


typedef struct {
	struct wl_display *display;

	swl_session_backend_t *session;
	swl_dev_man_backend_t *dev_man;
	swl_input_backend_t *input;
	swl_display_backend_t *backend;
	swl_renderer_t *renderer;

	swl_seat_t seat;

	swl_xdg_toplevel_t *active;

	struct wl_listener output_listner;

	struct wl_list clients;

	swl_texture_t *bg;

	server_ipc_sock ipc;
} soilleir_server_t;

typedef struct {
	soilleir_server_t *server;
	struct wl_listener destroy;
	struct wl_listener frame_listener;
} soilleir_output_t;


typedef struct swl_xdg_surface {
	swl_surface_t *swl_surface;
	
	soilleir_server_t *backend;
} swl_xdg_surface_t;

typedef struct swl_xdg_toplevel {
	swl_client_t *client;
	swl_xdg_surface_t *swl_xdg_surface;
	
	struct wl_list link;
} swl_xdg_toplevel_t;


void zswl_screenshot_manager_copy(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *output, 
		struct wl_resource *buffer, int32_t width, int32_t height,
		int32_t x, int32_t y) {
	swl_output_t *swl_output = wl_resource_get_user_data(output);
	struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(buffer);

	swl_output->copy(swl_output, shm_buffer, width, height, x, y);
}

static struct zswl_screenshot_manager_interface zswl_screenshot_impl = {
	.copy_output = zswl_screenshot_manager_copy,
};

static void zswl_screenshot_manager_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	
	resource = wl_resource_create(client, &zswl_screenshot_manager_interface, 1, id);
	wl_resource_set_implementation(resource, &zswl_screenshot_impl, data, NULL);
}

void wl_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
	swl_warn("handled surface destroy\n");
}

void wl_surf_destroy(struct wl_resource *resource) {
	swl_surface_t *swl_surface = wl_resource_get_user_data(resource);
	/*
	if(swl_surface->texture.data) {
		free(swl_surface->texture.data);
	}
	*/
	free(swl_surface);
}

void wl_surface_commit(struct wl_client *client, struct wl_resource *resource) {
	struct wl_shm_buffer *buffer;
	swl_surface_t *surface = wl_resource_get_user_data(resource);
	swl_xdg_surface_t *xdg_surface = wl_resource_get_user_data(surface->shell_surface);
	uint32_t width, height, format;	
	void *data;

	if(!surface->buffer) {
		xdg_surface_send_configure(surface->shell_surface, 0);
		return;
	}

	if(surface->texture) {
		xdg_surface->backend->renderer->destroy_texture(xdg_surface->backend->renderer, surface->texture);
		surface->texture = NULL;
	}

	buffer = wl_shm_buffer_get(surface->buffer);
	width = wl_shm_buffer_get_width(buffer);
	height = wl_shm_buffer_get_height(buffer);
	data = wl_shm_buffer_get_data(buffer);
	
	xdg_surface->backend->renderer->begin(xdg_surface->backend->renderer);
	surface->texture = xdg_surface->backend->renderer->create_texture(xdg_surface->backend->renderer, width, height, 0, data);

	xdg_surface->backend->renderer->end(xdg_surface->backend->renderer);
	swl_debug("New Texture: %p\n", surface->texture)
	wl_buffer_send_release(surface->buffer);
	if(surface->callback) {
		wl_callback_send_done(surface->callback, 0);
		wl_resource_destroy(surface->callback);
		surface->callback = NULL;
	}
}

void wl_surface_set_opaque_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}

void wl_surface_set_input_region(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *region) {

}

void wl_surface_frame(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	swl_surface_t *surface = wl_resource_get_user_data(resource);

	struct wl_resource *callback = wl_resource_create(client, &wl_callback_interface, 1, id);
	wl_resource_set_implementation(callback, NULL, NULL, NULL);

	surface->callback = callback;
}

void wl_surface_set_buffer_scale(struct wl_client *client, 
		struct wl_resource *resource, int32_t scale) {

}

void wl_surface_set_buffer_transform(struct wl_client *client, struct wl_resource *resource, int32_t transform) {

}

void wl_surface_attach(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *buffer, int32_t x, int32_t y) {
	swl_surface_t *surface = wl_resource_get_user_data(resource);
	surface->buffer = buffer;
}

void wl_surface_damage(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void wl_surface_damage_buffer(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void wl_surface_offset(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y) {

}

static const struct wl_surface_interface surface_implementation = {
		.set_opaque_region = wl_surface_set_opaque_region,
		.damage = wl_surface_damage,
		.damage_buffer = wl_surface_damage_buffer,
		.offset = wl_surface_offset,
		.set_buffer_scale = wl_surface_set_buffer_scale,
		.attach = wl_surface_attach,
		.frame = wl_surface_frame,
		.commit = wl_surface_commit,
		.destroy = wl_surface_destroy,
		.set_input_region = wl_surface_set_input_region,
		.set_buffer_transform = wl_surface_set_buffer_transform,
};

static void wl_compositor_create_surface(struct wl_client *client, struct wl_resource *compositor, uint32_t id) {
	struct wl_resource *surface;
	swl_surface_t *swl_surface = calloc(1, sizeof(swl_surface_t));
	
	surface = wl_resource_create(client, &wl_surface_interface, 6, id);
	wl_resource_set_implementation(surface, &surface_implementation, swl_surface, wl_surf_destroy);
	swl_surface->surface = surface;
}

static void wl_compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	wl_client_post_implementation_error(client, "This isn't implemented\n");
}

static const struct wl_compositor_interface compositor_interface = {
	.create_region = wl_compositor_create_region,
	.create_surface = wl_compositor_create_surface,
};

static void wl_compositor_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	


	resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	wl_resource_set_implementation(resource, &compositor_interface, data, NULL);
}

void swl_xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *toplevel) {
}

void swl_xdg_toplevel_handle_destroy(struct wl_resource *toplevel) {
	swl_xdg_toplevel_t *swl_xdg_toplevel;

	swl_xdg_toplevel = wl_resource_get_user_data(toplevel);	
	if(swl_xdg_toplevel->swl_xdg_surface->backend->active == swl_xdg_toplevel) {
		swl_xdg_toplevel->swl_xdg_surface->backend->active = NULL;
	}


	wl_list_remove(&swl_xdg_toplevel->link);
	free(swl_xdg_toplevel);
}

void swl_xdg_toplevel_set_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_unset_maximized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_minimized(struct wl_client *client, struct wl_resource *toplevel) {

}

void swl_xdg_toplevel_set_fullscreen(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *output) {

}

void swl_xdg_toplevel_set_parent(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *parent) {

}

void swl_xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *toplevel) {
	
}

void swl_xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *toplevel, 
		const char *title) {
	printf("Title set to %s\n", title);
}

void swl_xdg_toplevel_set_appid(struct wl_client *client, struct wl_resource *toplevel, 
		const char *appid) {

}

void swl_xdg_toplevel_set_max_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

void swl_xdg_toplevel_set_min_size(struct wl_client *client, struct wl_resource *toplevel, 
		int32_t width, int32_t height) {

}

void swl_xdg_toplevel_show_window_menu(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {

}

void swl_xdg_toplevel_move(struct wl_client *client, struct wl_resource *toplevel,
		struct wl_resource *seat, uint32_t serial) {

}

void swl_xdg_toplevel_resize(struct wl_client *client, struct wl_resource *toplevel,
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

void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
}

void xdg_surface_handle_destroy(struct wl_resource *resource) {
}


void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
		uint32_t id, struct wl_resource *xdg_surface, struct wl_resource *xdg_positioner) {

}

swl_client_t *swl_get_client(struct wl_client *client, struct wl_list *list) {
	swl_client_t *output;
	wl_list_for_each(output, list, link) {
		if(client == output->client) {
			return output;
		}
	}
	return NULL;
}

void swl_client_destroy(struct wl_listener *listener, void *data) {
	swl_client_t *client;

	client = wl_container_of(listener, client, destroy);
	
	wl_list_remove(&client->link);
	free(client);
}

swl_client_t *swl_get_client_or_create(struct wl_client *client, struct wl_list *list) {
	swl_client_t *output;
	wl_list_for_each(output, list, link) {
		if(client == output->client) {
			return output;
		}
	}
	
	output = calloc(1, sizeof(swl_client_t));
	output->client = client;
	output->destroy.notify = swl_client_destroy;
	wl_client_add_destroy_late_listener(output->client, &output->destroy);
	wl_list_insert(list, &output->link);
	wl_list_init(&output->surfaces);
	return output;
}

void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface,
		uint32_t id) {
	struct wl_resource *resource;
	struct swl_client *swl_client;
	swl_xdg_toplevel_t *swl_xdg_toplevel = calloc(1, sizeof(swl_xdg_toplevel_t));
	
	swl_xdg_toplevel->swl_xdg_surface = wl_resource_get_user_data(xdg_surface);

	resource = wl_resource_create(client, &xdg_toplevel_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_toplevel_impl, swl_xdg_toplevel, swl_xdg_toplevel_handle_destroy);
	
	swl_client = swl_get_client_or_create(client, &swl_xdg_toplevel->swl_xdg_surface->backend->clients);
	wl_list_insert(&swl_client->surfaces, &swl_xdg_toplevel->link);

	swl_xdg_toplevel->client = swl_client;
	if(swl_xdg_toplevel->swl_xdg_surface->backend->active == NULL) {
		swl_xdg_toplevel->swl_xdg_surface->backend->active = swl_xdg_toplevel;
	}

	struct wl_array array;
	wl_array_init(&array);

	xdg_toplevel_send_configure(resource, 0, 0, &array);
	xdg_toplevel_send_configure(resource, 1920, 1080, &array);
}

void xdg_surface_set_geometry(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource,
		uint32_t serial) {

}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy = xdg_surface_destroy,
	.set_window_geometry = xdg_surface_set_geometry,
	.get_popup = xdg_surface_get_popup,
	.get_toplevel = xdg_surface_get_toplevel,
	.ack_configure = xdg_surface_ack_configure,
};

void xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {

}

void xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {
	
}

void xdg_wm_base_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

void xdg_wm_base_create_surface(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface) {
	struct wl_resource *xdg_surface_resource;
	swl_surface_t *swl_surface;
	swl_xdg_surface_t *swl_xdg_surface = calloc(1, sizeof(swl_xdg_surface_t));

	swl_surface = wl_resource_get_user_data(surface);
	swl_xdg_surface->swl_surface = swl_surface;
	swl_xdg_surface->backend = wl_resource_get_user_data(resource);

	xdg_surface_resource = wl_resource_create(client, &xdg_surface_interface, 6, id);
	swl_surface->shell_surface = xdg_surface_resource;
	wl_resource_set_implementation(xdg_surface_resource, &xdg_surface_impl, swl_xdg_surface, xdg_surface_handle_destroy);
}


static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
		.pong = xdg_wm_base_pong,
		.get_xdg_surface = xdg_wm_base_create_surface,
		.destroy = xdg_wm_base_destroy,
		.create_positioner = xdg_wm_base_positioner,
};

static void xdg_wm_base_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	
	resource = wl_resource_create(client, &xdg_wm_base_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_wm_base_implementation, data, NULL);
}

void swl_keyboard_release(struct wl_client *client, struct wl_resource *resource) {

}

static const struct wl_keyboard_interface wl_keyboard_impl = {
	.release = swl_keyboard_release,
};

void wl_seat_release(struct wl_client *client, struct wl_resource *resource) {

}

void swl_switch_client(soilleir_server_t *server) {
	swl_client_t *clients;
	swl_xdg_toplevel_t *toplevel;

	bool next = false;

	wl_list_for_each(clients, &server->clients, link) {
		if(next) {
			wl_list_for_each(toplevel, &clients->surfaces, link) {
				server->active = toplevel;
				return;
			}/*This client didn't have a surface keep cycling*/
		}
		if(server->active && clients->client == server->active->client->client) {
			next = true;
		}

	}
	/*we reached the end of the list cycle back to the start*/

	wl_list_for_each(clients, &server->clients, link) {
		wl_list_for_each(toplevel, &clients->surfaces, link) {
			server->active = toplevel;
			return;
		}
	}
}

void wl_seat_key_press(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, key);
	soilleir_server_t *server = wl_container_of(seat, server, seat);
	swl_key_event_t *key = data;
	xkb_keysym_t sym;
	swl_info("Mods %d\n", xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_EFFECTIVE));
	key->key += 8;
	sym = xkb_state_key_get_one_sym(seat->state, key->key);
	if(xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED) == (MODIFER_ALT | MODIFER_CTRL) &&
			key->state) {
		swl_debug("%d %d\n", sym, XKB_KEY_XF86Switch_VT_4);
		switch(sym) {
			case XKB_KEY_Return:
				if(fork() == 0) {
					/*TODO swap to magma term*/
					execlp("havoc", "havoc", NULL);
					exit(1);
					return;
				}
				break;
			case XKB_KEY_Escape:
				wl_display_terminate(server->display);
				return;
				break;
			case XKB_KEY_Tab:
				swl_switch_client(server);
				break;
			case XKB_KEY_XF86Switch_VT_1:
			case XKB_KEY_XF86Switch_VT_2:
			case XKB_KEY_XF86Switch_VT_3:
			case XKB_KEY_XF86Switch_VT_4:
			case XKB_KEY_XF86Switch_VT_5:
			case XKB_KEY_XF86Switch_VT_6:
			case XKB_KEY_XF86Switch_VT_7:
			case XKB_KEY_XF86Switch_VT_8:
			case XKB_KEY_XF86Switch_VT_9:
			case XKB_KEY_XF86Switch_VT_10:
			case XKB_KEY_XF86Switch_VT_11:
			case XKB_KEY_XF86Switch_VT_12:
				swl_info("%d\n", sym - XKB_KEY_XF86Switch_VT_1);
				server->session->switch_vt(server->session, 1 + sym - XKB_KEY_XF86Switch_VT_1);
				return;
			case XKB_KEY_Left:
				if(server->active->swl_xdg_surface->swl_surface->x - 10 >= 0) {
					server->active->swl_xdg_surface->swl_surface->x -= 10;
				}
				return;
			case XKB_KEY_Right:
				if(server->active->swl_xdg_surface->swl_surface->x + 10 <= 1920) {
					server->active->swl_xdg_surface->swl_surface->x += 10;
				}
				return;
			case XKB_KEY_Up:
				if(server->active->swl_xdg_surface->swl_surface->y - 10 >= 0) {
					server->active->swl_xdg_surface->swl_surface->y -= 10;
				}
				return;
			case XKB_KEY_Down:
				if(server->active->swl_xdg_surface->swl_surface->y + 10 <= 1920) {
					server->active->swl_xdg_surface->swl_surface->y += 10;
				}
				return;

			default:
				break;
		
		}
	}

	xkb_state_update_key(seat->state, key->key, key->state ? XKB_KEY_DOWN : XKB_KEY_UP);
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED);


	if(server->active && server->active->client->keyboard) {
		wl_keyboard_send_modifiers(server->active->client->keyboard, 0, depressed, 0, 0, 0);
		wl_keyboard_send_key(server->active->client->keyboard, 0, 0, key->key - 8, key->state);
	}
	
}

void wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *keyboard;
	swl_client_t *swl_client;
	soilleir_server_t *backend = wl_resource_get_user_data(resource);
	char tmp[] = "/tmp/swlkeyfd-XXXXXX";

	swl_client = swl_get_client_or_create(client, &backend->clients);

	keyboard = wl_resource_create(client, &wl_keyboard_interface, 9, id);
	wl_resource_set_implementation(keyboard, &wl_keyboard_impl, NULL, NULL);

	char *map_str = xkb_keymap_get_as_string(backend->seat.map, XKB_KEYMAP_FORMAT_TEXT_V1);
	int fd = mkstemp(tmp);
	ftruncate(fd, strlen(map_str));
	write(fd, map_str, strlen(map_str));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(map_str));
	unlink(tmp);

	close(fd);
	free(map_str);

	swl_client->keyboard = keyboard;
}

void wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

void wl_seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}

static const struct wl_seat_interface wl_seat_impl = {
	.release = wl_seat_release,
	.get_pointer = wl_seat_get_pointer, 
	.get_keyboard = wl_seat_get_keyboard,
	.get_touch = wl_seat_get_touch,
};

static void wl_seat_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	soilleir_server_t *backend = data;
	resource = wl_resource_create(client, &wl_seat_interface, 9, id);
	wl_resource_set_implementation(resource, &wl_seat_impl, data, NULL);

	wl_seat_send_name(resource, backend->seat.seat_name);
	wl_seat_send_capabilities(resource, backend->seat.caps);
}

static void soilleir_frame(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, frame_listener);
	swl_xdg_toplevel_t *toplevel;
	swl_client_t *client;
	soilleir_server_t *server = soil_output->server;

	server->renderer->attach_output(soil_output->server->renderer, output);
	server->renderer->begin(soil_output->server->renderer);
	
	server->renderer->clear(server->renderer, 0.2f, 0.2f, 0.2f, 1.0f);
	if(server->bg) {
		server->renderer->draw_texture(server->renderer, server->bg, 0, 0);
	}

	swl_debug("%p %p %p\n", soil_output->server, soil_output, soil_output->server->active);
	wl_list_for_each(client, &soil_output->server->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			swl_debug("Texture in: %p\n", toplevel->swl_xdg_surface->swl_surface->texture);
			server->renderer->draw_texture(server->renderer, toplevel->swl_xdg_surface->swl_surface->texture, toplevel->swl_xdg_surface->swl_surface->x, toplevel->swl_xdg_surface->swl_surface->y);
		
		}
	}
	if(soil_output->server->active) {
	wl_list_for_each(toplevel, &soil_output->server->active->client->surfaces, link) {
			swl_debug("Texture in: %p\n", toplevel->swl_xdg_surface->swl_surface->texture);

			server->renderer->draw_texture(server->renderer, toplevel->swl_xdg_surface->swl_surface->texture, toplevel->swl_xdg_surface->swl_surface->x, toplevel->swl_xdg_surface->swl_surface->y);
		
	}
	}
	server->renderer->end(server->renderer);
}

static void soilleir_output_destroy(struct wl_listener *listener, void *data) {
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, destroy);
	
	free(soil_output);
}

static void soilleir_new_output(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = calloc(1, sizeof(soilleir_output_t));
	soilleir_server_t *server = wl_container_of(listener, server, output_listner);

	soil_output->frame_listener.notify = soilleir_frame;
	soil_output->destroy.notify = soilleir_output_destroy;
	soil_output->server = server;
	wl_signal_add(&output->frame, &soil_output->frame_listener);
	wl_signal_add(&output->destroy, &soil_output->destroy);

	server->renderer->attach_output(server->renderer, output);
}


typedef struct {
	uint32_t opcode;
	uint32_t len;
	int32_t height, width, stride, size;
	uint32_t depth;
	uint32_t format;
}__attribute__((packed)) swl_ipc_background_image;

int server_ipc(int32_t fd, uint32_t mask, void *data) {
	int client;
	soilleir_server_t *soilleir = data;
	swl_ipc_background_image *image;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);
	client = accept(fd, &addr, &len);

	printf("Accept %m\n");
	union {
    struct cmsghdr    cm;
    char              control[CMSG_SPACE(sizeof(int))];
  } control_un;
	server_ipc_msg_t mesg;
  struct msghdr msg = { 0 };
  struct iovec iov[1] = { 0 };
	char buf[4096] = { 0 };

	
	struct cmsghdr hdr; 
	
	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	iov[0].iov_len = 4095;
	iov[0].iov_base = buf;
	msg.msg_iov = iov;
  msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	
	if(recvmsg(client, &msg, 0) < 0) {
		swl_error("Erorr %d %m\n", client);
		return 0;
	}
  struct cmsghdr  *cmptr;
	int recvfd;

	if ((cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
    cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmptr->cmsg_level != SOL_SOCKET)
            printf("control level != SOL_SOCKET");
        if (cmptr->cmsg_type != SCM_RIGHTS)
            printf("control type != SCM_RIGHTS");
		recvfd = *((int *) CMSG_DATA(cmptr));
	}
	swl_debug("recvfd: %d\n", recvfd);
	image = (void*)buf;

	void *ptr = mmap(0, image->size, PROT_READ | PROT_WRITE, MAP_SHARED, recvfd, 0);
	soilleir->bg = soilleir->renderer->create_texture(soilleir->renderer, image->width, image->height, image->format, ptr);
	swl_debug("%p, %p bg\n", ptr, soilleir->bg);
	return 0;
}


int main(int argc, char **argv) {
	soilleir_server_t soilleir = {0};
	struct wl_client *client;
	struct wl_event_loop *loop;

	swl_log_init(SWL_LOG_INFO, "/tmp/soilleir");

	soilleir.display = wl_display_create();
	setenv("WAYLAND_DISPLAY", wl_display_add_socket_auto(soilleir.display), 1);
	/* temp display
	setenv("SWL_IPC_SOCKET", "/tmp/swl-srvipc", 1);
	
	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, "/tmp/swl-srvipc", 15);
	soilleir.ipc.fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(bind(soilleir.ipc.fd, (void*)&addr, sizeof(addr)) == -1) {
		swl_error("Failed to init IPC\n");
		return -1;
	}
	*/	
	loop = wl_display_get_event_loop(soilleir.display);
	/*
	wl_event_loop_add_fd(loop, soilleir.ipc.fd, WL_EVENT_READABLE, server_ipc, &soilleir);
	listen(soilleir.ipc.fd, 128);
	*/
	wl_global_create(soilleir.display, &wl_compositor_interface, 6, soilleir.display, wl_compositor_bind);
	wl_global_create(soilleir.display, &xdg_wm_base_interface, 6, &soilleir, xdg_wm_base_bind);
	wl_display_init_shm(soilleir.display);
	wl_global_create(soilleir.display, &wl_seat_interface, 9, &soilleir, wl_seat_bind);
	wl_global_create(soilleir.display, &zswl_screenshot_manager_interface, 1, NULL, zswl_screenshot_manager_bind);

	soilleir.session = swl_seatd_backend_create(soilleir.display);
	soilleir.dev_man = swl_udev_backend_create(soilleir.display);
	soilleir.input = swl_libinput_backend_create(soilleir.display, soilleir.session, soilleir.dev_man);
	soilleir.backend = swl_drm_create_backend(soilleir.display, soilleir.session);
	
	soilleir.seat.caps = WL_SEAT_CAPABILITY_KEYBOARD;
	soilleir.seat.seat_name = "seat0";
	soilleir.seat.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	soilleir.seat.map = xkb_keymap_new_from_names(soilleir.seat.xkb, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	soilleir.seat.state = xkb_state_new(soilleir.seat.map);
	soilleir.seat.key.notify = wl_seat_key_press;
	wl_signal_add(&soilleir.input->key, &soilleir.seat.key);

	wl_list_init(&soilleir.clients);
	soilleir.output_listner.notify = soilleir_new_output;
	wl_signal_add(&soilleir.backend->new_output, &soilleir.output_listner);

	soilleir.renderer = swl_egl_renderer_create_by_fd(soilleir.backend->get_drm_fd(soilleir.backend));
	swl_udev_backend_start(soilleir.dev_man);
	swl_drm_backend_start(soilleir.backend);
	wl_display_run(soilleir.display);

	wl_display_destroy_clients(soilleir.display);


	swl_drm_backend_stop(soilleir.backend);
	swl_drm_backend_destroy(soilleir.backend, soilleir.session);

	swl_libinput_backend_destroy(soilleir.input);
	swl_udev_backend_destroy(soilleir.dev_man);
	swl_seatd_backend_destroy(soilleir.session);
	
	xkb_state_unref(soilleir.seat.state);
	xkb_keymap_unref(soilleir.seat.map);
	xkb_context_unref(soilleir.seat.xkb);
	
	wl_display_destroy(soilleir.display);

	return 0;
}
