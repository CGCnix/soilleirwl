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

	void *pointer_data;
	void (*pointer_callback)(void *data, uint32_t mods, int32_t dx, int32_t dy);
	wl_fixed_t pointer_x;
	wl_fixed_t pointer_y;

	/*Focused USE SURFACES WL_CLIENT TO FIND seat client*/
	struct wl_resource *keyboard;
	struct wl_resource *pointer;

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

	void *pointer_data;
	void (*pointer_callback)(void *data, int32_t dx, int32_t dy);

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
	struct wl_list link;
} swl_seat_device_t;

void swl_keyboard_release(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void swl_seat_keyboard_resource_destroy(struct wl_resource *resource) {
	swl_seat_client_t *client = wl_resource_get_user_data(resource);

	client->keyboard = NULL;
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

	if(seat->keyboard) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			/*Send to all this clients Keyboard Listeners*/
			if(seat_client->client == wl_resource_get_client(seat->keyboard) && seat_client->keyboard) {
				wl_keyboard_send_modifiers(seat_client->keyboard, wl_display_next_serial(display), depressed, 0, 0, 0);
				wl_keyboard_send_key(seat_client->keyboard, wl_display_next_serial(display), 0, key->key, key->state);
			}
		}
	}
}

static void wl_pointer_release(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void swl_seat_pointer_resource_destroy(struct wl_resource *resource) {
	swl_seat_client_t *client = wl_resource_get_user_data(resource);

	client->pointer = NULL;
}

static void wl_pointer_set_cur(struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *surface, int32_t x, int32_t y) {

}

static void swl_seat_button(struct wl_listener *listener, void *data) {
	swl_seat_device_t *seat_dev = wl_container_of(listener, seat_dev, button);
	swl_seat_t *seat = seat_dev->seat;
	swl_button_event_t *button = data;
	swl_seat_client_t *seat_client;
	struct wl_display *display = wl_global_get_display(seat->global);

	if(seat->pointer) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			/*Send to all this clients Keyboard Listeners*/
			if(seat_client->client == wl_resource_get_client(seat->pointer) && seat_client->pointer) {
				wl_pointer_send_button(seat_client->pointer, wl_display_next_serial(wl_global_get_display(seat->global)), button->time, button->button, button->state);
				wl_pointer_send_frame(seat_client->pointer);
			}
		}
	}
}

static void swl_pointer_motion(struct wl_listener *listener, void *data) {
	swl_seat_device_t *seat_dev = wl_container_of(listener, seat_dev, motion);
	swl_seat_t *seat = seat_dev->seat;
	swl_motion_event_t *motion = data;
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(seat->state, XKB_STATE_MODS_DEPRESSED);
	swl_seat_client_t *seat_client;
	struct wl_display *display = wl_global_get_display(seat->global);

	if(seat->pointer_callback) {
		seat->pointer_callback(seat->pointer_data, depressed, motion->dx, motion->dy);
	}

	int32_t x = wl_fixed_to_int(seat->pointer_x);
	x += motion->dx;
	seat->pointer_x = wl_fixed_from_int(x);
	int32_t y = wl_fixed_to_int(seat->pointer_y);
	y += motion->dy;
	seat->pointer_y = wl_fixed_from_int(y);


	if(seat->pointer) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			/*Send to all this clients Keyboard Listeners*/
			if(seat_client->client == wl_resource_get_client(seat->pointer) && seat_client->pointer) {
				wl_pointer_send_motion(seat_client->pointer, motion->time, seat->pointer_x, seat->pointer_y);
				wl_pointer_send_frame(seat_client->pointer);
			}
		}
	}
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

	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == client && seat_client->seat == seat_resource) {
			break;
		}
	}

	keyboard = wl_resource_create(client, &wl_keyboard_interface, SWL_KEYBOARD_VERSION, id);
	wl_resource_set_implementation(keyboard, &swl_keyboard_impl, seat_client, swl_seat_keyboard_resource_destroy);

	keymap = xkb_keymap_get_as_string(seat->keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
	fd = mkstemp(tmp);
	
	ftruncate(fd, strlen(keymap));
	write(fd, keymap, strlen(keymap));
	wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, strlen(keymap));
	wl_keyboard_send_repeat_info(keyboard, 25, 600);
	unlink(tmp);

	close(fd);
	free(keymap);

	if(seat_client) {
		seat_client->keyboard = keyboard;
	}
}

static void swl_seat_get_pointer(struct wl_client *client, struct wl_resource *seat_resource, uint32_t id) {
	struct wl_resource *pointer;	
	swl_seat_client_t *seat_client;
	swl_seat_t *seat = wl_resource_get_user_data(seat_resource);

	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == client && seat_client->seat == seat_resource) {
			break;
		}
	}

	pointer = wl_resource_create(client, &wl_pointer_interface, SWL_POINTER_VERSION, id);
	wl_resource_set_implementation(pointer, &wl_pointer_impl, seat_client, swl_seat_pointer_resource_destroy);
	if(seat_client) {
		seat_client->pointer = pointer;
	}
}

static void swl_seat_get_touch(struct wl_client *client, struct wl_resource *resource, uint32_t id) {

}

static void swl_seat_release(struct wl_client *client, struct wl_resource *resource) {
	/*This will lead to swl_seat_resource_destroy being called*/
	wl_resource_destroy(resource);
}

static void swl_seat_resource_destroy(struct wl_resource *seat_resource) {	
	struct wl_client *client = wl_resource_get_client(seat_resource);
	swl_seat_t *seat = wl_resource_get_user_data(seat_resource);
	swl_seat_client_t *seat_client;
	
	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == client && seat_client->seat == seat_resource) {
			break; /*Return this*/
		}
	}
	
	if(!seat_client) {
		/* I don't think this is "possible" outside of maybe race conditions and other
		 * weirdness but in theroy there should always be a seat_client if a seat
		 * was bound
		 */
		swl_warn("Seat resource destroyed without client\n");
		return;
	}
	
	/*remove this from the list of clients*/
	wl_list_remove(&seat_client->link);
	/*TODO support multiple cursors and keyboards in a single seat*/
	if(seat_client->pointer) {
		wl_resource_destroy(seat_client->pointer);
	}

	if(seat_client->keyboard) {
		wl_resource_destroy(seat_client->keyboard);
	}
	free(seat_client);
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

	if(seat->keyboard) {
		wl_list_for_each(client, &seat->clients, link) {
			if(client->client == wl_resource_get_client(seat->keyboard) && client->keyboard) {
				wl_keyboard_send_enter(client->keyboard, wl_display_next_serial(display), seat->keyboard, &keys);
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
	seat_dev->button.notify = swl_seat_button;

	wl_signal_add(&seat_dev->input->motion, &seat_dev->motion);
	wl_signal_add(&seat_dev->input->key, &seat_dev->key);
	wl_signal_add(&seat_dev->input->button, &seat_dev->button);

	wl_list_insert(&seat->devices, &seat_dev->link);
}

static void swl_seat_disable(struct wl_listener *listener, void *data) {
	swl_seat_t *seat = wl_container_of(listener, seat, disable);
	swl_seat_client_t *client;
	struct wl_display *display = wl_global_get_display(seat->global);
	struct wl_array keys;
	wl_array_init(&keys);

	if(seat->keyboard) {
		wl_list_for_each(client, &seat->clients, link) {
			if(client->client == wl_resource_get_client(seat->keyboard) && client->keyboard) {
				wl_keyboard_send_leave(client->keyboard, wl_display_next_serial(display), seat->keyboard);
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

	resource = wl_resource_create(client, &wl_seat_interface, version, id);
	wl_resource_set_implementation(resource, &wl_seat_impl, data, swl_seat_resource_destroy);
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

void swl_seat_set_focused_surface_keyboard(swl_seat_t *seat, struct wl_resource *resource) {
	swl_seat_client_t *seat_client;
	struct wl_array keys;
	wl_array_init(&keys);

	if(seat->keyboard) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			if(seat_client->client == wl_resource_get_client(seat->keyboard) && seat_client->keyboard) {
				wl_keyboard_send_leave(seat_client->keyboard, wl_display_next_serial(wl_global_get_display(seat->global)), seat->keyboard);
			}
		}
	}

	seat->keyboard = resource;
	if(seat->keyboard == NULL) return;
	wl_list_for_each(seat_client, &seat->clients, link) {
		if(seat_client->client == wl_resource_get_client(seat->keyboard) && seat_client->keyboard) {
			wl_keyboard_send_enter(seat_client->keyboard, wl_display_next_serial(wl_global_get_display(seat->global)), seat->keyboard, &keys);
		}
	}
}

void swl_seat_set_focused_surface_pointer(swl_seat_t *seat, struct wl_resource *resource, wl_fixed_t x, wl_fixed_t y) {
	swl_seat_client_t *seat_client;
	struct wl_array keys;
	wl_array_init(&keys);


	if(seat->pointer) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			if(seat_client->client == wl_resource_get_client(seat->pointer) && seat_client->pointer) {
				wl_pointer_send_leave(seat_client->pointer, wl_display_next_serial(wl_global_get_display(seat->global)), seat->pointer);
			}
		}
	}

	seat->pointer = resource;
	seat->pointer_x = x;
	seat->pointer_y = y;
	if(seat->pointer) {
		wl_list_for_each(seat_client, &seat->clients, link) {
			if(seat_client->client == wl_resource_get_client(seat->pointer) && seat_client->pointer) {
				wl_pointer_send_enter(seat_client->pointer, wl_display_next_serial(wl_global_get_display(seat->global)), seat->pointer, x, y);
			}
		}
	}
}

int swl_seat_add_pointer_callback(swl_seat_t *seat, void (*callback)(void *data, uint32_t mods, int32_t dx, int32_t dy), void *data) {
	seat->pointer_callback = callback;
	seat->pointer_data = data;
	return 0;
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

void swl_seat_destroy(swl_seat_t *seat) {
	swl_seat_binding_t *binding, *tmp;
	swl_seat_device_t *device, *dev_tmp;
	wl_list_remove(&seat->new_input.link);
	wl_list_remove(&seat->disable.link);
	wl_list_remove(&seat->activate.link);

	wl_list_for_each_safe(binding, tmp, &seat->bindings, link) {
		wl_list_remove(&binding->link);
		free(binding);
	}

	/*TODO: if a server wants to delete and remake the seat at run time
	 * the new seat wouldn't get the device added events so seat's have to be
	 * made at the start and destroyed at the end not sure if we want or need to
	 * change this but we maybe should just because someone may have a use for seats
	 * created at runtime maybe adding a enumerate devices call for backend
	 */
	wl_list_for_each_safe(device, dev_tmp, &seat->devices, link) {
		wl_list_remove(&device->link);
		wl_list_remove(&device->button.link);
		wl_list_remove(&device->key.link);
		wl_list_remove(&device->motion.link);
		free(device);
	}

	xkb_state_unref(seat->state);
	xkb_keymap_unref(seat->keymap);
	xkb_context_unref(seat->xkb);

	wl_global_destroy(seat->global);

	free(seat);
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
