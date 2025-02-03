#include "soilleirwl/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include <soilleirwl/logger.h>
#include <soilleirwl/backend/backend.h>
#include <soilleirwl/interfaces/swl_seat.h>
#include <soilleirwl/interfaces/swl_output.h>
#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_compositor.h>

#include <private/xdg-shell-server.h>
#include <private/swl-screenshot-server.h>

#include "./ipc.h"
#include "./minsoilleir.h"

typedef struct soilleir_surface {
	soilleir_server_t *soilleir;
	swl_surface_t *swl_surface;
	soilleir_output_t *output;
	struct wl_listener commit;
	struct wl_listener destroy;
	
	struct wl_list link;
} soilleir_surface_t;

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


void zswl_screenshot_manager_copy(struct wl_client *client, 
		struct wl_resource *resource, struct wl_resource *output, 
		struct wl_resource *buffer, int32_t width, int32_t height,
		int32_t x, int32_t y) {
	swl_output_t *swl_output = wl_resource_get_user_data(output);
	struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get(buffer);
	void *data = wl_shm_buffer_get_data(shm_buffer);

	swl_output->renderer->attach_target(swl_output->renderer, swl_output->targets[swl_output->front_buffer]);
	swl_output->renderer->begin(swl_output->renderer);
	swl_output->renderer->copy_from(swl_output->renderer, data, wl_shm_buffer_get_height(shm_buffer), wl_shm_buffer_get_width(shm_buffer),
			x, y, wl_shm_buffer_get_format(shm_buffer));
	swl_output->renderer->end(swl_output->renderer);
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
	wl_resource_destroy(toplevel);
}

void swl_xdg_toplevel_handle_destroy(struct wl_resource *toplevel) {
	swl_xdg_toplevel_t *swl_xdg_toplevel;
	swl_client_t *client;

	swl_xdg_toplevel = wl_resource_get_user_data(toplevel);	
	if (swl_xdg_toplevel->backend->active == swl_xdg_toplevel) {
		swl_xdg_toplevel->backend->active = NULL;
		swl_seat_set_focused_surface_keyboard(swl_xdg_toplevel->backend->seat, NULL);
		swl_seat_set_focused_surface_pointer(swl_xdg_toplevel->backend->seat, NULL, 0, 0);

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


void swl_xdg_surface_send_configure(void *data);

void swl_xdg_toplevel_set_parent(struct wl_client *client, struct wl_resource *toplevel, struct wl_resource *parent) {
	swl_xdg_toplevel_t *swl_xdg_toplevel = wl_resource_get_user_data(toplevel);
	struct wl_display *display = wl_client_get_display(client);

	if(swl_xdg_toplevel->swl_xdg_surface->idle_configure == NULL) {
		swl_xdg_toplevel->swl_xdg_surface->idle_configure = wl_event_loop_add_idle(wl_display_get_event_loop(display),
				swl_xdg_surface_send_configure, swl_xdg_toplevel->swl_xdg_surface);
	} 

}

void swl_xdg_toplevel_unset_fullscreen(struct wl_client *client, struct wl_resource *toplevel) {
	
}

void swl_xdg_toplevel_set_title(struct wl_client *client, struct wl_resource *toplevel, 
		const char *title) {
	swl_xdg_toplevel_t *top = wl_resource_get_user_data(toplevel);
	top->title = strdup(title);
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
	wl_resource_destroy(resource);
}

void xdg_surface_handle_destroy(struct wl_resource *resource) {
	swl_xdg_surface_t *surface = wl_resource_get_user_data(resource);
	free(surface);
}


void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *resource,
	uint32_t id, struct wl_resource *xdg_surface, struct wl_resource *xdg_positioner) {
}

void xdg_surface_toplevel_configure(struct wl_client *client, swl_xdg_surface_t *surface, struct wl_resource *resource) {
	struct wl_array states;
	wl_array_init(&states);

	xdg_toplevel_send_configure(resource, surface->swl_surface->width, surface->swl_surface->height, &states);
}	

void swl_xdg_surface_send_configure(void *data) {
	swl_xdg_surface_t *surface = data;

	surface->idle_configure = NULL;

	surface->role->configure(wl_resource_get_client(surface->role_resource), surface, surface->role_resource);
	xdg_surface_send_configure(surface->swl_surface->role_resource, 100);
}

static swl_xdg_surface_role_t swl_xdg_surface_toplevel_role = {
	.configure = xdg_surface_toplevel_configure,
};

void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface,
		uint32_t id) {
	struct wl_resource *resource;
	struct swl_client *swl_client;
	swl_xdg_toplevel_t *swl_xdg_toplevel = calloc(1, sizeof(swl_xdg_toplevel_t));
	soilleir_server_t *server;
	struct wl_array keys;
	wl_array_init(&keys);
	soilleir_output_t *output;
	struct wl_array array;
	wl_array_init(&array);
	
	swl_xdg_toplevel->swl_xdg_surface = wl_resource_get_user_data(xdg_surface);

	server = swl_xdg_toplevel->swl_xdg_surface->backend;

	resource = wl_resource_create(client, &xdg_toplevel_interface, 6, id);
	wl_resource_set_implementation(resource, &xdg_toplevel_impl, swl_xdg_toplevel, swl_xdg_toplevel_handle_destroy);

	swl_client = swl_get_client_or_create(client, &swl_xdg_toplevel->swl_xdg_surface->backend->clients);
	
	wl_list_insert(&swl_client->surfaces, &swl_xdg_toplevel->link);

	swl_xdg_toplevel->client = swl_client;
	swl_xdg_toplevel->swl_xdg_surface->role = &swl_xdg_surface_toplevel_role;
	swl_xdg_toplevel->swl_xdg_surface->role_resource = resource;
	swl_xdg_toplevel->backend = server;
	swl_xdg_toplevel->swl_xdg_surface->swl_surface->width = 640;
	swl_xdg_toplevel->swl_xdg_surface->swl_surface->height = 480;
	xdg_toplevel_send_wm_capabilities(resource, &array);
}

void xdg_surface_set_geometry(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {
}

void xdg_surface_ack_configure(struct wl_client *client, struct wl_resource *resource,
		uint32_t serial) {
	swl_xdg_surface_t *surface = wl_resource_get_user_data(resource);
	swl_client_t *swl_client = swl_get_client_or_create(client, &surface->backend->clients);
}

static const struct xdg_surface_interface xdg_surface_impl = {
	.destroy = xdg_surface_destroy,
	.set_window_geometry = xdg_surface_set_geometry,
	.get_popup = xdg_surface_get_popup,
	.get_toplevel = xdg_surface_get_toplevel,
	.ack_configure = xdg_surface_ack_configure,
};

void swl_xdg_surface_precommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {
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

void swl_xdg_surface_postcommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {

}

static swl_surface_role_t swl_xdg_surface_role = {	
	.postcommit = swl_xdg_surface_postcommit,
	.precommit = swl_xdg_surface_precommit,
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
	swl_surface->role_resource = xdg_surface_resource;
	swl_surface->role = &swl_xdg_surface_role;
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

void soilleir_quit(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	struct wl_display *display = data;
	wl_display_terminate(display);
}

void soilleir_pointer_motion(void *data, uint32_t mods, int32_t dx, int32_t dy) {
	soilleir_server_t *soilleir = data;
	swl_client_t *client;
	swl_xdg_toplevel_t *toplevel;
	struct wl_array states;
	wl_array_init(&states);
	soilleir->xpos += dx;
	soilleir->ypos += dy;
	int found = 0;

	soilleir->backend->BACKEND_MOVE_CURSOR(soilleir->backend, soilleir->xpos, soilleir->ypos);

	wl_list_for_each(client, &soilleir->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			swl_surface_t *surface = toplevel->swl_xdg_surface->swl_surface;
			if(soilleir->xpos >= surface->position.x && soilleir->xpos <= surface->position.x + surface->width &&
				 soilleir->ypos >= surface->position.y && soilleir->ypos <= surface->position.y + surface->height) {
				
				if(soilleir->active != toplevel) {
					swl_seat_set_focused_surface_keyboard(soilleir->seat, toplevel->swl_xdg_surface->swl_surface->resource);
				}
				
				if(soilleir->pointer_surface != toplevel) {
					swl_seat_set_focused_surface_pointer(soilleir->seat, toplevel->swl_xdg_surface->swl_surface->resource,
							wl_fixed_from_int(soilleir->xpos - surface->position.x),	wl_fixed_from_int(soilleir->ypos - surface->position.y));
				}


				soilleir->active = toplevel;
				soilleir->pointer_surface = toplevel;
				found = 1;
				break;
			}
		}
		if(found) {
			break;
		}
	}

	if(!found) {
		soilleir->pointer_surface = NULL;
		swl_seat_set_focused_surface_pointer(soilleir->seat, NULL, 0, 0);
	}

	if(mods == (SWL_MOD_CTRL)) {
		if(soilleir->active) {
			soilleir->active->swl_xdg_surface->swl_surface->position.y += dy;
			soilleir->active->swl_xdg_surface->swl_surface->position.x += dx;
		}
	} else if(mods == (SWL_MOD_ALT)) {
		if(soilleir->active) {
			soilleir->active->swl_xdg_surface->swl_surface->width += dx;
			soilleir->active->swl_xdg_surface->swl_surface->height += dy;
		
			if(soilleir->active->swl_xdg_surface->idle_configure == NULL) {
				soilleir->active->swl_xdg_surface->idle_configure = wl_event_loop_add_idle(wl_display_get_event_loop(soilleir->display),
				swl_xdg_surface_send_configure, soilleir->active->swl_xdg_surface);
			}
		}
	}

}

void soilleir_switch_session(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	soilleir_server_t *soilleir = data;

	if(!state) {
		return;
	}
	soilleir->backend->BACKEND_SWITCH_VT(soilleir->backend, 1 + sym - XKB_KEY_XF86Switch_VT_1);
}

void soilleir_next_client(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	soilleir_server_t *soilleir = data;
	swl_client_t *client;
	swl_xdg_toplevel_t *toplevel;

	bool next = false;
	wl_list_for_each(client, &soilleir->clients, link) {
		if(next) {
			wl_list_for_each(toplevel, &client->surfaces, link) {
				soilleir->active = toplevel;
				swl_seat_set_focused_surface_keyboard(soilleir->seat, soilleir->active->swl_xdg_surface->swl_surface->resource);
				return;
			}/*This client didn't have a surface keep cycling*/
		}
		if(soilleir->active && client->client == soilleir->active->client->client) {
			next = true;
		}
	}
	
	wl_list_for_each(client, &soilleir->clients, link) {
		wl_list_for_each(toplevel, &client->surfaces, link) {
			soilleir->active = toplevel;
			swl_seat_set_focused_surface_keyboard(soilleir->seat, soilleir->active->swl_xdg_surface->swl_surface->resource);
			return;
		}
	}
}

void soilleir_spawn(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	if(!state) return;
	if(fork() == 0) {
		execlp(data, data, NULL);
	}
}

static void swl_surface_render(swl_surface_t *surface, swl_output_t *output) {
	swl_subsurface_t *subsurface;

	if(surface->texture) {
		output->renderer->draw_texture(output->renderer, surface->texture, 
				surface->position.x - output->x, surface->position.y - output->y,
				surface->position.x, surface->position.y);
		if(surface->frame) {
			wl_callback_send_done(surface->frame, 0);
			wl_resource_destroy(surface->frame);
			surface->frame = NULL;
		}	
	}

	wl_list_for_each(subsurface, &surface->subsurfaces, link) {
		if(subsurface->surface->texture) {
			output->renderer->draw_texture(output->renderer, subsurface->surface->texture,
				(surface->position.x - output->x) + subsurface->position.x, 
				(surface->position.y - output->y) + subsurface->position.y, 
				(surface->position.x) + subsurface->position.x, 
				(surface->position.y) + subsurface->position.y);
		}
		if(subsurface->surface->frame) {
			wl_callback_send_done(subsurface->surface->frame, 0);
			wl_resource_destroy(subsurface->surface->frame);
			subsurface->surface->frame = NULL;
		}
	}
}

static void soilleir_frame(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, frame_listener);
	swl_xdg_toplevel_t *toplevel;
	swl_subsurface_t *subsurface;
	swl_client_t *client;
	soilleir_server_t *server = soil_output->server;
	output->renderer->attach_target(output->renderer, output->targets[output->front_buffer]);
	output->renderer->begin(output->renderer);
	
	output->renderer->clear(output->renderer, 0.2f, 0.2f, 0.2f, 1.0f);
	if(output->background) {
		output->renderer->draw_texture(output->renderer, output->background, 0, 0, 0, 0);
	}

	wl_list_for_each(client, &soil_output->server->clients, link) {
		if(client == server->active->client) continue;
		wl_list_for_each(toplevel, &client->surfaces, link) {
			swl_surface_render(toplevel->swl_xdg_surface->swl_surface, output);
		}
	}

	if(soil_output->server->active) {
		wl_list_for_each(toplevel, &soil_output->server->active->client->surfaces, link) {
			swl_surface_render(toplevel->swl_xdg_surface->swl_surface, output);		
		}
	}
	output->renderer->end(output->renderer);
}

static void soilleir_output_bind(struct wl_listener *listener, void *data) {
	struct wl_resource *output = data;
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, bind);
	soilleir_server_t *server = soil_output->server;
	swl_client_t *client = swl_get_client_or_create(wl_resource_get_client(output), &server->clients);

	if(client->client == wl_resource_get_client(output)) {
		client->output = output;
	}
}

static void soilleir_output_destroy(struct wl_listener *listener, void *data) {
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, destroy);
	
	wl_list_remove(&soil_output->link);
	free(soil_output);
}



static void soilleir_new_output(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = calloc(1, sizeof(soilleir_output_t));
	soilleir_server_t *server = wl_container_of(listener, server, output_listner);
	soil_output->bind.notify = soilleir_output_bind;
	soil_output->frame_listener.notify = soilleir_frame;
	soil_output->destroy.notify = soilleir_output_destroy;
	soil_output->server = server;
	soil_output->common = data;

	wl_signal_add(&output->frame, &soil_output->frame_listener);
	wl_signal_add(&output->destroy, &soil_output->destroy);
	wl_signal_add(&output->bind, &soil_output->bind);
	wl_list_insert(&server->outputs, &soil_output->link);
}

static void swl_pointer_precommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {
		
}

static void swl_pointer_postcommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {

}

static swl_surface_role_t swl_cursor_surface_role = {
	.postcommit = swl_pointer_postcommit,
	.precommit = swl_pointer_precommit,
};

void soilleir_set_cursor_callback(void *data, struct wl_resource *pointer, struct wl_resource *surface_res, int32_t dx, int32_t dy) {
	soilleir_server_t *server = data;
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);
	
	surface->role = &swl_cursor_surface_role;
	surface->role_resource = pointer;

	swl_client_t *client = swl_get_client_or_create(wl_resource_get_client(surface_res), &server->clients);
	client->cursor = surface_res;
}

void swl_create_data_dev_man(struct wl_display *display);

void soilleir_surface_destroy(struct wl_listener *listener, void *data) {
	soilleir_surface_t *surface = wl_container_of(listener, surface, destroy);

	wl_list_remove(&surface->commit.link);
	wl_list_remove(&surface->destroy.link);

	free(surface);
}

void soilleir_surface_commit(struct wl_listener *listener, void *data) {
	soilleir_surface_t *surface = wl_container_of(listener, surface, commit);
	soilleir_output_t *output = NULL;
	int found = 0;
	struct wl_shm_buffer *shm_buffer;
	int32_t width, height;
	uint32_t format;


	wl_list_for_each(output, &surface->soilleir->outputs, link) {
		break;
	}

	if(!surface->swl_surface->buffer.buffer) return;

	if(surface->swl_surface->texture) {
		surface->output->common->renderer->destroy_texture(surface->output->common->renderer, surface->swl_surface->texture);
		surface->swl_surface->texture = NULL;
	}

	shm_buffer = wl_shm_buffer_get(surface->swl_surface->buffer.buffer);
	width = wl_shm_buffer_get_width(shm_buffer);
	height = wl_shm_buffer_get_height(shm_buffer);
	format = wl_shm_buffer_get_format(shm_buffer);
	wl_shm_buffer_begin_access(shm_buffer);

	surface->swl_surface->texture = output->common->renderer->create_texture(output->common->renderer, width, height, format, wl_shm_buffer_get_data(shm_buffer));
	surface->output = output;

	wl_shm_buffer_end_access(shm_buffer);
	wl_buffer_send_release(surface->swl_surface->buffer.buffer);
}

void soilleir_new_surface(struct wl_listener *listener, void *data) {
	swl_surface_t *surface = data;
	soilleir_server_t *soilleir = wl_container_of(listener, soilleir, new_surface);
	soilleir_surface_t *soilleir_surf = calloc(1, sizeof(soilleir_surface_t));

	soilleir_surf->swl_surface = surface;
	soilleir_surf->soilleir = soilleir;
	
	soilleir_surf->commit.notify = soilleir_surface_commit;
	soilleir_surf->destroy.notify = soilleir_surface_destroy;
	
	wl_signal_add(&surface->commit, &soilleir_surf->commit);
	wl_signal_add(&surface->destroy, &soilleir_surf->destroy);
}

int main(int argc, char **argv) {
	soilleir_server_t soilleir = {0};
	struct wl_client *client;
	struct wl_event_loop *loop;
	const char *kmap = "de";
	soilleir_output_t *output, *tmp;

	if(getenv("SWL_KEYMAP")) {
		kmap = getenv("SWL_KEYMAP");
	}
	swl_log_init(SWL_LOG_INFO, "/tmp/soilleir");
	soilleir.display = wl_display_create();
	setenv("WAYLAND_DISPLAY", wl_display_add_socket_auto(soilleir.display), 1);
	soilleir_ipc_init(&soilleir);

	wl_list_init(&soilleir.outputs);

	wl_global_create(soilleir.display, &xdg_wm_base_interface, 3, &soilleir, xdg_wm_base_bind);
	wl_display_init_shm(soilleir.display);
	wl_global_create(soilleir.display, &zswl_screenshot_manager_interface, 1, NULL, zswl_screenshot_manager_bind);
	
	soilleir.backend = swl_backend_create_by_env(soilleir.display);

	soilleir.compositor = swl_compositor_create(soilleir.display, &soilleir);
	soilleir.new_surface.notify = soilleir_new_surface;
	wl_signal_add(&soilleir.compositor->new_surface, &soilleir.new_surface);
	soilleir.subcompositor = swl_subcompositor_create(soilleir.display);

	soilleir.seat = swl_seat_create(soilleir.display, soilleir.backend, "seat0", kmap);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_Escape, soilleir_quit, soilleir.display);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_Return, soilleir_spawn, "foot");
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_Tab, soilleir_next_client, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_1, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_2, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_3, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_4, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_5, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_6, soilleir_switch_session, &soilleir);
	swl_seat_add_pointer_callback(soilleir.seat, soilleir_pointer_motion, &soilleir);
	swl_seat_add_set_cursor_callback(soilleir.seat, soilleir_set_cursor_callback, &soilleir);

	wl_list_init(&soilleir.clients);

	soilleir.output_listner.notify = soilleir_new_output;
	soilleir.backend->BACKEND_ADD_NEW_OUTPUT_LISTENER(soilleir.backend, &soilleir.output_listner);

	swl_create_data_dev_man(soilleir.display);
	soilleir.backend->BACKEND_START(soilleir.backend);
	wl_display_run(soilleir.display);
	soilleir.backend->BACKEND_STOP(soilleir.backend);
	wl_display_destroy_clients(soilleir.display);
	
	swl_seat_destroy(soilleir.seat);
	swl_subcompositor_destroy(soilleir.subcompositor);
	swl_compositor_destroy(soilleir.compositor);

	soilleir_ipc_deinit(&soilleir);
	soilleir.backend->BACKEND_DESTROY(soilleir.backend);
	wl_list_for_each_safe(output, tmp, &soilleir.outputs, link) {
		wl_list_remove(&output->link);
		free(output);
	}

	wl_display_destroy(soilleir.display);

	return 0;
}
