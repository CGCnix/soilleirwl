#include "../src/xdg-shell-server.h"
#include "../src/swl-screenshot-server.h"
#include "soilleirwl/display.h"
#include "soilleirwl/interfaces/swl_compositor.h"
#include "soilleirwl/x11.h"
#include <errno.h>
#include <soilleirwl/session.h>
#include <soilleirwl/input.h>
#include <soilleirwl/dev_man.h>
#include <soilleirwl/logger.h>


#include <soilleirwl/interfaces/swl_output.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_compositor.h>

#include <xkbcommon/xkbcommon.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>

#define MODIFER_CTRL 4
#define MODIFER_ALT 8

enum {
	SERVER_CHG_KEYBMAP,
	SERVER_SET_BACKGRN,
};

typedef struct{
	uint32_t opcode;
	uint32_t len;
}__attribute__((packed)) soilleir_ipc_msg_t;

typedef struct {
	uint32_t opcode;
	uint32_t len;
	int32_t height, width, stride, size;
	uint32_t depth;
	uint32_t format;
}__attribute__((packed)) soilleir_ipc_background_image;

typedef struct {
	uint32_t opcode;
	uint32_t len;
	uint16_t layout;
}__attribute__((packed)) soilleir_ipc_change_keymap;

typedef struct {
	int fd;
	int lock;
	struct wl_event_source *source;
} server_ipc_sock;

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
	struct wl_listener disable;
	struct wl_listener activate;
	struct wl_listener motion;

	struct xkb_context *xkb;
	struct xkb_keymap *map;
	struct xkb_state *state;
} swl_seat_t;

typedef struct swl_xdg_toplevel swl_xdg_toplevel_t;

typedef struct {
	struct wl_display *display;
	/*
	swl_session_backend_t *session;
	swl_dev_man_backend_t *dev_man;
	swl_input_backend_t *input;
	swl_display_backend_t *backend;
	*/
	swl_x11_backend_t *backend;

	swl_seat_t seat;

	swl_xdg_toplevel_t *active;

	struct wl_listener output_listner;
	struct wl_listener output_listner2;
	struct wl_list clients;

	void *bg;

	struct wl_list outputs;

	server_ipc_sock ipc;
} soilleir_server_t;

typedef struct {
	soilleir_server_t *server;
	swl_output_t *common;

	struct wl_listener destroy;
	struct wl_listener frame_listener;
	struct wl_list link;
} soilleir_output_t;


typedef struct swl_xdg_surface {
	swl_surface_t *swl_surface;
	
	soilleir_server_t *backend;
	struct wl_resource *role;
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

void xdg_toplevel_mapped(struct wl_resource *surf, struct wl_client *wl_client) {
	swl_surface_t *surface = wl_resource_get_user_data(surf);
	swl_xdg_surface_t *xdg_surface = wl_resource_get_user_data(surface->role);
	swl_xdg_toplevel_t *xdg_toplevel = wl_resource_get_user_data(xdg_surface->role);
	swl_client_t *client = swl_get_client_or_create(wl_client, &xdg_surface->backend->clients);


	struct wl_array keys;
	wl_array_init(&keys);

	if(xdg_surface->backend->active == NULL) {
		xdg_surface->backend->active = xdg_toplevel;

		if(client == xdg_surface->backend->active->client && client->keyboard) {
			wl_keyboard_send_enter(client->keyboard, 20, surf, &keys);
		}
	}
}

void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface,
		uint32_t id) {
	struct wl_resource *resource;
	struct swl_client *swl_client;
	swl_xdg_toplevel_t *swl_xdg_toplevel = calloc(1, sizeof(swl_xdg_toplevel_t));
	soilleir_server_t *server;
	struct wl_array keys;
	wl_array_init(&keys);


	swl_xdg_toplevel->swl_xdg_surface = wl_resource_get_user_data(xdg_surface);

	server = swl_xdg_toplevel->swl_xdg_surface->backend;

	resource = wl_resource_create(client, &xdg_toplevel_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_toplevel_impl, swl_xdg_toplevel, swl_xdg_toplevel_handle_destroy);
	
	swl_client = swl_get_client_or_create(client, &swl_xdg_toplevel->swl_xdg_surface->backend->clients);
	wl_list_insert(&swl_client->surfaces, &swl_xdg_toplevel->link);

	swl_xdg_toplevel->client = swl_client;

	struct wl_array array;
	wl_array_init(&array);
	swl_xdg_toplevel->swl_xdg_surface->role = resource;
	xdg_toplevel_send_configure(resource, 640, 480, &array);
	swl_xdg_toplevel->swl_xdg_surface->swl_surface->width = 640;
	swl_xdg_toplevel->swl_xdg_surface->swl_surface->height = 480;
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
	swl_surface->role = xdg_surface_resource;
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
	
	if(wl_list_empty(&server->clients)) {
		return;
	}

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
	struct wl_array keys;
	wl_array_init(&keys);
	key->key += 8;

	sym = xkb_state_key_get_one_sym(seat->state, key->key);
	if(xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED) == (MODIFER_ALT | MODIFER_CTRL) &&
			key->state) {
		switch(sym) {
			case XKB_KEY_Return:
				if(fork() == 0) {
					/*TODO swap to magma term*/
					execlp("foot", "foot", "-d", "info", NULL);
					swl_error("Spawning food failed: %m\n");
					exit(1);
					return;
				}
				return; 
				break;
			case XKB_KEY_Escape:
				wl_display_terminate(server->display);
				return;
				break;
			case XKB_KEY_Tab:
				if(server->active && server->active->client->keyboard) {
					wl_keyboard_send_leave(server->active->client->keyboard, 1, server->active->swl_xdg_surface->swl_surface->resource);
				}
				swl_switch_client(server);
				if(server->active && server->active->client->keyboard) {
					wl_keyboard_send_enter(server->active->client->keyboard, 1, server->active->swl_xdg_surface->swl_surface->resource, &keys);
				}
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
				//server->session->switch_vt(server->session, 1 + sym - XKB_KEY_XF86Switch_VT_1);
				return;
			case XKB_KEY_Left:
				if(server->active && server->active->swl_xdg_surface->swl_surface->position.x - 10 >= 0) {
					server->active->swl_xdg_surface->swl_surface->position.x -= 10;
				}
				return;
			case XKB_KEY_Right:
			case XKB_KEY_Z:
				if(server->active) {	
					server->active->swl_xdg_surface->swl_surface->position.x += 10;
				}
				return;
			case XKB_KEY_Up:
				if(server->active && server->active->swl_xdg_surface->swl_surface->position.y - 10 >= 0) {
					server->active->swl_xdg_surface->swl_surface->position.y -= 10;
				}
				return;
			case XKB_KEY_Down:
				if(server->active && server->active->swl_xdg_surface->swl_surface->position.y + 10 <= 1920) {
					server->active->swl_xdg_surface->swl_surface->position.y += 10;
				}
				return;

			default:
				break;
		
		}
	}

	xkb_state_update_key(seat->state, key->key, key->state ? XKB_KEY_DOWN : XKB_KEY_UP);
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED);


	if(server->active && server->active->client->keyboard) {
		swl_xdg_toplevel_t *toplevel;
		struct wl_array array;
		wl_array_init(&array);
		wl_list_for_each(toplevel, &server->active->client->surfaces, link) {
			break;
		}
		wl_keyboard_send_modifiers(server->active->client->keyboard, 1, depressed, 0, 0, 0);
		wl_keyboard_send_key(server->active->client->keyboard, 1, 0, key->key - 8, key->state);
	}
	
}

void wl_seat_get_keyboard(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *keyboard;
	swl_client_t *swl_client;
	struct wl_array keys;
	wl_array_init(&keys);
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
	wl_keyboard_send_repeat_info(keyboard, 25, 600);
	unlink(tmp);

	close(fd);
	free(map_str);

	swl_client->keyboard = keyboard;
	if(backend->active && backend->active->client == swl_client) {
		wl_keyboard_send_enter(swl_client->keyboard, wl_display_next_serial(backend->display), backend->active->swl_xdg_surface->swl_surface->resource, &keys);
	}
}


static void wl_pointer_release(struct wl_client *client, struct wl_resource *resource) {

}

static void wl_pointer_set_cur(struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *surface, int32_t x, int32_t y) {

}

static void swl_pointer_motion(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, motion);
	soilleir_server_t *server = wl_container_of(seat, server, seat); 
	swl_client_t *client;
	swl_xdg_toplevel_t *toplevel;
	swl_pointer_event_t *pointer = data;
	struct wl_array keys;
	wl_array_init(&keys);

	printf("abs %d %d, delta %d %d\n", pointer->absy, pointer->absx, pointer->dy, pointer->dx);
	wl_list_for_each(client, &server->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			swl_surface_t *surface = toplevel->swl_xdg_surface->swl_surface;
			if(pointer->absx >= surface->position.x && pointer->absx <= surface->position.x + surface->width &&
				 pointer->absy >= surface->position.y && pointer->absy <= surface->position.y + surface->height) {
				if(server->active && server->active->client->keyboard) {
					wl_keyboard_send_leave(server->active->client->keyboard, wl_display_next_serial(wl_client_get_display(server->active->client->client)), server->active->swl_xdg_surface->swl_surface->resource);
				}

				server->active = toplevel;
				if(server->active && server->active->client->keyboard) {
					wl_keyboard_send_enter(server->active->client->keyboard, wl_display_next_serial(wl_client_get_display(server->active->client->client)), server->active->swl_xdg_surface->swl_surface->resource, &keys);
				}
			}
		}
	}

	if(xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED) == (MODIFER_CTRL)) {
		if(server->active) {
			server->active->swl_xdg_surface->swl_surface->position.y += pointer->dy;
			server->active->swl_xdg_surface->swl_surface->position.x += pointer->dx;
		}
	} else if(xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED) == (MODIFER_ALT)) {
		if(server->active) {
			server->active->swl_xdg_surface->swl_surface->width += pointer->dy;
			server->active->swl_xdg_surface->swl_surface->height += pointer->dx;
			xdg_toplevel_send_configure(server->active->swl_xdg_surface->role, 
					server->active->swl_xdg_surface->swl_surface->width,
					server->active->swl_xdg_surface->swl_surface->height,
					&keys);
			xdg_surface_send_configure(server->active->swl_xdg_surface->swl_surface->role, 20);
		}
	}



}

static const struct wl_pointer_interface wl_pointer_impl = {
	.release = wl_pointer_release,
	.set_cursor = wl_pointer_set_cur,
};

void wl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *pointer;
	
	pointer = wl_resource_create(client, &wl_pointer_interface, 9, id);
	wl_resource_set_implementation(pointer, &wl_pointer_impl, NULL, NULL);
	
}

void wl_seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}

static const struct wl_seat_interface wl_seat_impl = {
	.release = wl_seat_release,
	.get_pointer = wl_seat_get_pointer, 
	.get_keyboard = wl_seat_get_keyboard,
	.get_touch = wl_seat_get_touch,
};

static void swl_seat_activate(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, activate);
	soilleir_server_t *server = wl_container_of(seat, server, seat);
	struct  wl_array keys;

	wl_array_init(&keys);

	if(server->active && server->active->client->keyboard) {
		wl_keyboard_send_enter(server->active->client->keyboard, wl_display_next_serial(server->display), server->active->swl_xdg_surface->swl_surface->resource, &keys);
	}

}

static void swl_seat_disable(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, disable);
	soilleir_server_t *server = wl_container_of(seat, server, seat);
	struct  wl_array keys;

	wl_array_init(&keys);

	if(server->active && server->active->client->keyboard) {
		wl_keyboard_send_leave(server->active->client->keyboard, wl_display_next_serial(server->display), server->active->swl_xdg_surface->swl_surface->resource);
	}

}

static void wl_seat_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	soilleir_server_t *backend = data;
	resource = wl_resource_create(client, &wl_seat_interface, 9, id);
	wl_resource_set_implementation(resource, &wl_seat_impl, data, NULL);

	if(version >= WL_SEAT_NAME_SINCE_VERSION) {
		wl_seat_send_name(resource, backend->seat.seat_name);
	}
	if(version >= WL_SEAT_CAPABILITIES_SINCE_VERSION) {
		wl_seat_send_capabilities(resource, backend->seat.caps);
	}
}

static void xdg_toplevel_render(swl_surface_t *toplevel, swl_output_t *output) {
	swl_subsurface_t *subsurface;
	/*Draw the toplevel*/
	if(toplevel->texture) {
		toplevel->renderer->draw_texture(toplevel->renderer, toplevel->texture, 
				toplevel->position.x - output->x, toplevel->position.y - output->y);
	}

	/*Draw the subsurface*/
	/*TODO: Z level*/
	wl_list_for_each(subsurface, &toplevel->subsurfaces, link) {
		if(subsurface->surface->texture) {
			toplevel->renderer->draw_texture(toplevel->renderer, subsurface->surface->texture,
				(toplevel->position.x - output->x) + subsurface->position.x, 
				(toplevel->position.y - output->y) + subsurface->position.y);
		}
	}
}

static void soilleir_frame(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, frame_listener);
	swl_xdg_toplevel_t *toplevel;
	swl_client_t *client;
	soilleir_server_t *server = soil_output->server;
	output->renderer->attach_output(output->renderer, output);
	output->renderer->begin(output->renderer);
	
	output->renderer->clear(output->renderer, 0.2f, 0.2f, 0.2f, 1.0f);
	if(output->background) {
		output->renderer->draw_texture(output->renderer, output->background, 0, 0);
	}

	wl_list_for_each(client, &soil_output->server->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			xdg_toplevel_render(toplevel->swl_xdg_surface->swl_surface, output);
		}
	}

	if(soil_output->server->active) {
		wl_list_for_each(toplevel, &soil_output->server->active->client->surfaces, link) {
			xdg_toplevel_render(toplevel->swl_xdg_surface->swl_surface, output);		
		}
	}
	output->renderer->end(output->renderer);
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
	soil_output->common = data;
	wl_signal_add(&output->frame, &soil_output->frame_listener);
	wl_signal_add(&output->destroy, &soil_output->destroy);

	wl_list_insert(&server->outputs, &soil_output->link);

	output->renderer->attach_output(output->renderer, output);
}



int soilleir_ipc_set_bgimage(struct msghdr *msg, soilleir_server_t *soilleir) {
	soilleir_ipc_background_image *image = msg->msg_iov[0].iov_base;
	int recvfd = -1;
  struct cmsghdr *cmptr;

	if ((cmptr = CMSG_FIRSTHDR(msg)) != NULL &&
    cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
    if (cmptr->cmsg_level != SOL_SOCKET && cmptr->cmsg_type != SCM_RIGHTS) {
			swl_error("IPC Client sent a cmsg without SOL_SOCKET or SCM_RIGHTS\n");
			return 0;
		}
		recvfd = *((int *) CMSG_DATA(cmptr));
	}
	if(recvfd < 0) {
		swl_error("IPC recv fd for bg image invalid\n");
		return 0;
	}

	soilleir->bg = mmap(0, image->size, PROT_READ | PROT_WRITE, MAP_SHARED, recvfd, 0);
	close(recvfd);
	soilleir_output_t *output;
	wl_list_for_each(output, &soilleir->outputs, link) {
		output->common->background = output->common->renderer->create_texture(output->common->renderer, image->width, image->height, image->format, soilleir->bg);
	}

	munmap(soilleir->bg, image->size);
	return 0;	
}

int soilleir_ipc_chg_keymap(struct msghdr *msg, soilleir_server_t *soilleir) {
	soilleir_ipc_change_keymap *keymap = msg->msg_iov->iov_base;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	struct xkb_rule_names names;
	char layout[3] = { 0 };

	layout[0] = keymap->layout >> 8;
	layout[1] = keymap->layout & 0xff;

	swl_debug("Layout sent: %s\n", layout);

	names.rules = NULL;
	names.model = NULL;
	names.options = NULL;
	names.variant = NULL;
	names.layout = layout;

	xkb_map = xkb_keymap_new_from_names(soilleir->seat.xkb, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	xkb_state = xkb_state_new(xkb_map);
	/*TODO: I should probably copy the state to the new state but testing fir now*/
	swl_client_t *client = NULL;
	wl_list_for_each(client, &soilleir->clients, link) {
		if(client->keyboard) {
			char tmp[] = "/tmp/swlkeyfd-XXXXXX";
			char *map_str = xkb_keymap_get_as_string(xkb_map, XKB_KEYMAP_FORMAT_TEXT_V1);
			int fd = mkstemp(tmp);
			ftruncate(fd, strlen(map_str));
			write(fd, map_str, strlen(map_str));
			wl_keyboard_send_keymap(client->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(map_str));
			unlink(tmp);
			close(fd);
			free(map_str);
		}
	}

	xkb_state_unref(soilleir->seat.state);
	xkb_keymap_unref(soilleir->seat.map);

	soilleir->seat.map = xkb_map;
	soilleir->seat.state = xkb_state;
	return 0;
}

int server_ipc(int32_t fd, uint32_t mask, void *data) {
	int client, recvfd;
	soilleir_server_t *soilleir = data;
	soilleir_ipc_msg_t *ipcmsg;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);
	client = accept(fd, &addr, &len);

	union {
    struct cmsghdr    cm;
    char              control[CMSG_SPACE(sizeof(int))];
  } control_un;
  struct msghdr msg = { 0 };
  struct iovec iov[1] = { 0 };
	char buf[4096] = { 0 };
	
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
	/*HACK WE KINDA JUST ASSUME WE GOT THE WHOLE MESSAGE*/
	close(client);
	ipcmsg = (void*)buf;
	
	switch (ipcmsg->opcode) {
		case SERVER_SET_BACKGRN:
			return soilleir_ipc_set_bgimage(&msg, soilleir);
		case SERVER_CHG_KEYBMAP:
			return soilleir_ipc_chg_keymap(&msg, soilleir);
	}

	return 0;
}

#define SOILLEIR_IPC_PATH "%s/soil-%d"
#define SOILLEIR_IPC_LOCK "%s/soil-%d.lock"

int soilleir_ipc_deinit(soilleir_server_t *soilleir) {
	char lock_addr[256] = { 0 };
	struct sockaddr_un addr = { 0 };
	pid_t pid = getpid();
	const char *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");

	snprintf(addr.sun_path, sizeof(addr.sun_path), SOILLEIR_IPC_PATH, xdg_rt_dir, pid);
	snprintf(lock_addr, sizeof(lock_addr), SOILLEIR_IPC_LOCK, xdg_rt_dir, pid);
	
	wl_event_source_remove(soilleir->ipc.source);


	unlink(addr.sun_path);
	close(soilleir->ipc.fd);	
	flock(soilleir->ipc.lock, LOCK_UN | LOCK_NB);
	close(soilleir->ipc.lock);
	remove(lock_addr);
	
	return 0;
}

int soilleir_ipc_init(soilleir_server_t *soilleir) {
	char lock_addr[256] = { 0 };
	struct sockaddr_un addr = { 0 };
	struct stat stat;
	pid_t pid = getpid();
	const char *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
	struct wl_event_loop *loop;

	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), SOILLEIR_IPC_PATH, xdg_rt_dir, pid);
	snprintf(lock_addr, sizeof(lock_addr), SOILLEIR_IPC_LOCK, xdg_rt_dir, pid);
	soilleir->ipc.lock = open(lock_addr, O_CREAT | O_CLOEXEC | O_RDWR,
														(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
	if(soilleir->ipc.lock < 0) {
		swl_error("Unabled to open lock file %s: %s\n", lock_addr, strerror(errno));
		return -1;
	}

	/*If we can lock this file we can then unlink the IPC socket should it exist*/
	if(flock(soilleir->ipc.lock, LOCK_EX | LOCK_NB) < 0) {
		/* Due to fact we use the pid in the lock file this shouldn't be 
		 * possible ever as if this process is killed the lock will be released
		 * by the OS. It should only happen if we call this function twice
		 */
		swl_error("Failed to lock file %s: %s\n", lock_addr, strerror(errno));
		return -1;
	}

	if(lstat(addr.sun_path, &stat) < 0 && errno != ENOENT) {
		/*If we failed to stat the file for some reason other than 
		 * this socket not existing
		 */
		swl_error("Failed to state file %s: %s\n", addr.sun_path, strerror(errno));
		return -1;
	}	else {
		/*Success so stale old socket exists*/
		unlink(addr.sun_path);
	}

	soilleir->ipc.fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(bind(soilleir->ipc.fd, (void*)&addr, sizeof(addr)) == -1) {
		swl_error("Failed bind to socket %s: %s\n", addr.sun_path, strerror(errno));
		return -1;
	}
	loop = wl_display_get_event_loop(soilleir->display);
	
	soilleir->ipc.source = wl_event_loop_add_fd(loop, soilleir->ipc.fd, WL_EVENT_READABLE, server_ipc, soilleir);
	
	listen(soilleir->ipc.fd, 128);
	setenv("SWL_IPC_SOCKET", addr.sun_path, 1);	
	return 0;
}

void swl_create_data_dev_man(struct wl_display *display);

int main(int argc, char **argv) {
	soilleir_server_t soilleir = {0};
	struct wl_client *client;
	struct wl_event_loop *loop;
	const char *drm_device = "/dev/dri/card0";
	swl_log_init(SWL_LOG_INFO, "/tmp/soilleir");

	soilleir.display = wl_display_create();
	setenv("WAYLAND_DISPLAY", wl_display_add_socket_auto(soilleir.display), 1);
	
	soilleir_ipc_init(&soilleir);

	wl_list_init(&soilleir.outputs);

	wl_global_create(soilleir.display, &xdg_wm_base_interface, 6, &soilleir, xdg_wm_base_bind);
	wl_display_init_shm(soilleir.display);
	wl_global_create(soilleir.display, &wl_seat_interface,
			9, &soilleir, wl_seat_bind);
	wl_global_create(soilleir.display, &zswl_screenshot_manager_interface, 1, NULL, zswl_screenshot_manager_bind);
	/*
	soilleir.session = swl_seatd_backend_create(soilleir.display);
	if(soilleir.session == NULL) {
		swl_error("Failed to create session\n");
		return 1;
	}

	soilleir.dev_man = swl_udev_backend_create(soilleir.display);
	if(soilleir.dev_man == NULL) {
		swl_error("Failed to create device manager\n");
		return 1;
	}

	soilleir.input = swl_libinput_backend_create(soilleir.display, soilleir.session, soilleir.dev_man);
	if(soilleir.input == NULL) {
		swl_error("Failed to create input backend");
		return 1;
	}

	if(getenv("SWL_DRM_DEVICE")) {
		drm_device = getenv("SWL_DRM_DEVICE");
	}
	soilleir.backend = swl_drm_create_backend(soilleir.display, soilleir.session, drm_device);
	if(soilleir.backend == NULL) {
		swl_error("Failed to create display backend\n");
		return 1;
	}	
	*/
	
	soilleir.backend = swl_x11_backend_create(soilleir.display);

	soilleir.seat.caps = WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	soilleir.seat.seat_name = "seat0";
	soilleir.seat.xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	soilleir.seat.map = xkb_keymap_new_from_names(soilleir.seat.xkb, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	soilleir.seat.state = xkb_state_new(soilleir.seat.map);
	soilleir.seat.key.notify = wl_seat_key_press;
	soilleir.seat.activate.notify = swl_seat_activate;
	soilleir.seat.disable.notify = swl_seat_disable;
	soilleir.seat.motion.notify = swl_pointer_motion;	
	wl_signal_add(&soilleir.backend->pointer, &soilleir.seat.motion);
	wl_signal_add(&soilleir.backend->key, &soilleir.seat.key);
	wl_signal_add(&soilleir.backend->disable, &soilleir.seat.disable);
	wl_signal_add(&soilleir.backend->activate, &soilleir.seat.activate);


	wl_list_init(&soilleir.clients);

	soilleir.output_listner.notify = soilleir_new_output;
	wl_signal_add(&soilleir.backend->new_output, &soilleir.output_listner);

	swl_create_compositor(soilleir.display, soilleir.backend->get_backend_renderer(soilleir.backend));
	swl_create_sub_compositor(soilleir.display);

	swl_create_data_dev_man(soilleir.display);
	/*
	swl_udev_backend_start(soilleir.dev_man);
	swl_drm_backend_start(soilleir.backend);
	*/
	swl_x11_backend_start(soilleir.backend);
	wl_display_run(soilleir.display);

	wl_display_destroy_clients(soilleir.display);
	/*
	swl_drm_backend_destroy(soilleir.backend, soilleir.session);

	swl_libinput_backend_destroy(soilleir.input);
	swl_udev_backend_destroy(soilleir.dev_man);
	swl_seatd_backend_destroy(soilleir.session);
	*/
	xkb_state_unref(soilleir.seat.state);
	xkb_keymap_unref(soilleir.seat.map);
	xkb_context_unref(soilleir.seat.xkb);
	
	soilleir_ipc_deinit(&soilleir);
	swl_x11_backend_destroy(soilleir.backend);	
	wl_display_destroy(soilleir.display);

	return 0;
}
