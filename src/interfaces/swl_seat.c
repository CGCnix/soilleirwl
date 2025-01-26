#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include <soilleirwl/interfaces/swl_input_device.h>
#include <soilleirwl/interfaces/swl_seat.h>
#include <soilleirwl/backend/backend.h>
#include <soilleirwl/logger.h>

#define SWL_SEAT_VERSION 9
#define SWL_POINTER_VERSION 9
#define SWL_KEYBOARD_VERSION 9
#define SWL_TOUCH_VERSION 9

struct swl_seat {
	struct wl_global *global;
	char *name;
	uint32_t caps;

	/*Focused USE SURFACES WL_CLIENT TO FIND seat client*/
	struct wl_resource *surface;

	struct wl_list devices;
	struct wl_list bindings;

	struct wl_list clients;

	struct xkb_context *xkb;
	struct xkb_keymap *keymap;
	struct xkb_state *state;

	struct wl_listener activate;
	struct wl_listener disable;
	struct wl_listener new_input;
};

typedef struct swl_seat_client {
	/*client is used to tell surface is part of this seat*/
	struct wl_client *client;
	/*seat resource is used to distingush when one client binds a seat
	 * twice or more*/
	struct wl_resource *seat;

	struct wl_resource *keyboard;
	struct wl_resource *pointer;

	struct wl_list link;
} swl_seat_client_t;

typedef struct swl_binding {
	xkb_mod_mask_t mods;
	uint32_t key;
	void *data;
	void (*callback)(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state);
	struct wl_list link;
} swl_seat_binding_t;

typedef struct swl_seat_device {
	struct wl_listener key;
	struct wl_listener motion;
	struct wl_listener button;

	swl_input_dev_t *input;
	swl_seat_t *seat;
	struct wl_list *link;
} swl_seat_device_t;

void swl_keyboard_release(struct wl_client *client, struct wl_resource *resource) {

}

static const struct wl_keyboard_interface swl_keyboard_impl = {
	.release = swl_keyboard_release,
};

void swl_seat_key_press(struct wl_listener *listener, void *data) {
	swl_seat_device_t *seat_dev = wl_container_of(listener, seat_dev, key);
	swl_seat_t *seat = seat_dev->seat;
	swl_seat_client_t *seat_client;
	swl_seat_binding_t *binding;
	swl_key_event_t *key = (swl_key_event_t*)data;
	xkb_state_update_key(seat->state, key->key+8, key->state ? XKB_KEY_DOWN : XKB_KEY_UP);

	xkb_mod_mask_t depressed = xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED);
	xkb_keysym_t sym = xkb_state_key_get_one_sym(seat->state, key->key+8);
	struct wl_display *display = wl_global_get_display(seat->global);
	/*callback the compositor*/
	wl_list_for_each(binding, &seat->bindings, link) {
		if(depressed == binding->mods && sym == binding->key && binding->callback) {
			binding->callback(binding->data, depressed, sym, key->state);
			return;
		}
	}

	if(seat->surface) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			if(seat_client->client == wl_resource_get_client(seat->surface) && seat_client->keyboard) {
				wl_keyboard_send_modifiers(seat_client->keyboard, wl_display_next_serial(display), depressed, 0, 0, 0);
				wl_keyboard_send_key(seat_client->keyboard, wl_display_next_serial(display), 0, key->key, key->state);
			}
		}
	}
}

static void wl_pointer_release(struct wl_client *client, struct wl_resource *resource) {

}

static void wl_pointer_set_cur(struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *surface, int32_t x, int32_t y) {

}

static void swl_pointer_motion(struct wl_listener *listener, void *data) {
	swl_seat_device_t *seat_dev = wl_container_of(listener, seat_dev, motion);
	swl_seat_t *seat = seat_dev->seat;
	

	/*
	soilleir_server_t *server = wl_container_of(seat, server, seat); 
	swl_client_t *client;
	swl_xdg_toplevel_t *toplevel;
	swl_motion_event_t *pointer = data;
	struct wl_array keys;
	wl_array_init(&keys);
	
	seat->x += pointer->dx;
	seat->y += pointer->dy;
	
	server->backend->BACKEND_MOVE_CURSOR(server->backend, seat->x, seat->y);

	swl_debug("abs %d %d, delta %d %d\n", pointer->absy, pointer->absx, pointer->dy, pointer->dx);
	wl_list_for_each(client, &server->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			swl_surface_t *surface = toplevel->swl_xdg_surface->swl_surface;
			if(seat->x >= surface->position.x && seat->x <= surface->position.x + surface->width &&
				 seat->y >= surface->position.y && seat->y <= surface->position.y + surface->height) {
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
			server->active->swl_xdg_surface->swl_surface->width += pointer->dx;
			server->active->swl_xdg_surface->swl_surface->height += pointer->dy;
			xdg_toplevel_send_configure(server->active->swl_xdg_surface->role, 
					server->active->swl_xdg_surface->swl_surface->width,
					server->active->swl_xdg_surface->swl_surface->height,
					&keys);
			xdg_surface_send_configure(server->active->swl_xdg_surface->swl_surface->role, wl_display_next_serial(server->display));
		}
	}
	*/
}

static const struct wl_pointer_interface wl_pointer_impl = {
	.release = wl_pointer_release,
	.set_cursor = wl_pointer_set_cur,
};

static void swl_seat_get_keyboard(struct wl_client *client, struct wl_resource *seat_resource, uint32_t id) {
	struct wl_resource *keyboard;
	swl_seat_client_t *seat_client;
	swl_seat_t *seat = wl_resource_get_user_data(seat_resource);
	char tmp[] = "/tmp/swlkeyfd-XXXXXX";
	char *keymap;
	struct wl_array keys;
	int fd;

	wl_array_init(&keys);

	keyboard = wl_resource_create(client, &wl_keyboard_interface, SWL_KEYBOARD_VERSION, id);
	wl_resource_set_implementation(keyboard, &swl_keyboard_impl, NULL, NULL);

	keymap = xkb_keymap_get_as_string(seat->keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	fd = mkstemp(tmp);
	
	ftruncate(fd, strlen(keymap));
	write(fd, keymap, strlen(keymap));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(keymap));
	wl_keyboard_send_repeat_info(keyboard, 25, 600);
	unlink(tmp);

	close(fd);
	free(keymap);

	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == client && seat_client->seat == seat_resource) {
			seat_client->keyboard = keyboard;
		}
	}
}

static void swl_seat_get_pointer(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *pointer;
	
	pointer = wl_resource_create(client, &wl_pointer_interface, SWL_POINTER_VERSION, id);
	wl_resource_set_implementation(pointer, &wl_pointer_impl, NULL, NULL);
	
}

static void swl_seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

static void swl_seat_release(struct wl_client *client, struct wl_resource *resource) {

}


static const struct wl_seat_interface wl_seat_impl = {
	.release = swl_seat_release,
	.get_pointer = swl_seat_get_pointer, 
	.get_keyboard = swl_seat_get_keyboard,
	.get_touch = swl_seat_get_touch,
};

static void swl_seat_activate(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, activate);
	swl_seat_client_t *client;
	struct wl_display *display = wl_global_get_display(seat->global);
	struct wl_array keys;
	wl_array_init(&keys);

	if(seat->surface) {
		wl_list_for_each(client, &seat->clients, link) {
			if(client->client == wl_resource_get_client(seat->surface) && client->keyboard) {
				wl_keyboard_send_enter(client->keyboard, wl_display_next_serial(display), seat->surface, &keys);
			}
		}
	}
}

static void swl_seat_new_device(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, new_input);
	swl_seat_device_t *seat_dev = calloc(1, sizeof(swl_seat_device_t));

	seat_dev->input = data;
	seat_dev->seat = seat;
	seat_dev->motion.notify = swl_pointer_motion;
	seat_dev->key.notify = swl_seat_key_press;

	wl_signal_add(&seat_dev->input->motion, &seat_dev->motion);
	wl_signal_add(&seat_dev->input->key, &seat_dev->key);
}

static void swl_seat_disable(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, disable);
	swl_seat_client_t *client;
	struct wl_display *display = wl_global_get_display(seat->global);
	struct wl_array keys;
	wl_array_init(&keys);

	if(seat->surface) {
		wl_list_for_each(client, &seat->clients, link) {
			if(client->client == wl_resource_get_client(seat->surface) && client->keyboard) {
				wl_keyboard_send_leave(client->keyboard, wl_display_next_serial(display), seat->surface);
			}
		}
	}
}

static void swl_seat_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *resource;
	swl_seat_t *seat = (swl_seat_t*)data;
	swl_seat_client_t *seat_client = calloc(1, sizeof(swl_seat_client_t));

	wl_list_insert(&seat->clients, &seat_client->link);

	resource = wl_resource_create(client, &wl_seat_interface, SWL_SEAT_VERSION, id);
	wl_resource_set_implementation(resource, &wl_seat_impl, data, NULL);
	seat_client->client = client;
	seat_client->seat = resource;

	if(version >= WL_SEAT_NAME_SINCE_VERSION) {
		wl_seat_send_name(resource, seat->name);
	}
	if(version >= WL_SEAT_CAPABILITIES_SINCE_VERSION) {
		wl_seat_send_capabilities(resource, seat->caps);
	}
}

void swl_seat_set_keymap(swl_seat_t *seat, char *map) {
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	struct xkb_rule_names names;
	swl_seat_client_t *seat_client;

	swl_debug("Layout sent: %s\n", map);

	names.rules = NULL;
	names.model = NULL;
	names.options = NULL;
	names.variant = NULL;
	names.layout = map;

	xkb_map = xkb_keymap_new_from_names(seat->xkb, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	xkb_state = xkb_state_new(xkb_map);
	/*TODO: I should probably copy the state to the new state but testing fir now*/
	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->keyboard) {
			char tmp[] = "/tmp/swlkeyfd-XXXXXX";
			char *map_str = xkb_keymap_get_as_string(xkb_map, XKB_KEYMAP_FORMAT_TEXT_V1);
			int fd = mkstemp(tmp);
			ftruncate(fd, strlen(map_str));
			write(fd, map_str, strlen(map_str));
			wl_keyboard_send_keymap(seat_client->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(map_str));
			unlink(tmp);
			close(fd);
			free(map_str);
		}
	}

	xkb_state_unref(seat->state);
	xkb_keymap_unref(seat->keymap);

	seat->keymap = xkb_map;
	seat->state = xkb_state;
}

void swl_seat_set_focused_surface(swl_seat_t *seat, struct wl_resource *resource) {
	swl_seat_client_t *seat_client;
	struct wl_array keys;
	wl_array_init(&keys);

	if(seat->surface) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			if(seat_client->client == wl_resource_get_client(seat->surface) && seat_client->keyboard) {
				wl_keyboard_send_leave(seat_client->keyboard, wl_display_next_serial(wl_global_get_display(seat->global)), seat->surface);
			}
		}
	}

	seat->surface = resource;
	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == wl_resource_get_client(seat->surface) && seat_client->keyboard) {
			wl_keyboard_send_enter(seat_client->keyboard, wl_display_next_serial(wl_global_get_display(seat->global)), seat->surface, &keys);
		}
	}
}

int swl_seat_add_binding(swl_seat_t *seat, xkb_mod_mask_t mods, xkb_keysym_t key, void (*callback)(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state), void *data) {
	swl_seat_binding_t *binding = calloc(1, sizeof(swl_seat_binding_t));
	if(!binding) return -1;

	binding->key = key;
	binding->data = data;
	binding->callback = callback;
	binding->mods = mods;
	wl_list_insert(&seat->bindings, &binding->link);
	return 0;
}

swl_seat_t *swl_seat_create(struct wl_display *display, swl_backend_t *backend, const char *name, const char *kmap) {
	swl_seat_t *seat = calloc(1, sizeof(swl_seat_t) + strlen(name) + 1);

	struct xkb_rule_names names;
	swl_seat_client_t *seat_client;

	names.rules = NULL;
	names.model = NULL;
	names.options = NULL;
	names.variant = NULL;
	names.layout = kmap;


	seat->global = wl_global_create(display, &wl_seat_interface, SWL_SEAT_VERSION, seat, swl_seat_bind);
	seat->name = (char *)&seat[1];
	memcpy(seat->name, name, strlen(name));
	seat->caps = WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER;
	seat->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	seat->keymap = xkb_keymap_new_from_names(seat->xkb, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
	seat->state = xkb_state_new(seat->keymap);
	seat->activate.notify = swl_seat_activate;
	seat->disable.notify = swl_seat_disable;
	seat->new_input.notify = swl_seat_new_device;
	backend->BACKEND_ADD_NEW_INPUT_LISTENER(backend, &seat->new_input);
	backend->BACKEND_ADD_DISABLE_LISTENER(backend, &seat->disable);
	backend->BACKEND_ADD_ACTIVATE_LISTENER(backend, &seat->activate);

	wl_list_init(&seat->bindings);
	wl_list_init(&seat->clients);
	wl_list_init(&seat->devices);

	return seat;
}
