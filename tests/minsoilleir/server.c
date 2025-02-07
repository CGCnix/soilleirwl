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
#include <soilleirwl/renderer.h>
#include <soilleirwl/backend/backend.h>
#include <soilleirwl/interfaces/swl_seat.h>
#include <soilleirwl/interfaces/swl_output.h>
#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_compositor.h>
#include <soilleirwl/interfaces/swl_xdg_shell.h>
#include <soilleirwl/interfaces/swl_data_dev_man.h>

#include <private/xdg-shell-server.h>
#include "swl-screenshot-server.h"

#include "./ipc.h"
#include "./minsoilleir.h"

typedef struct soilleir_xdg_surface {
	soilleir_server_t *soilleir;
	swl_xdg_surface_t *surface;

	struct wl_listener destroy;
	struct wl_listener new_toplevel;
	struct wl_listener new_popup;
} soilleir_xdg_surface_t;

typedef struct soilleir_surface {
	soilleir_server_t *soilleir;
	swl_surface_t *swl_surface;
	soilleir_output_t *output;
	struct wl_listener commit;
	struct wl_listener destroy;
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
	swl_output->renderer->copy_from(swl_output->renderer, data, wl_shm_buffer_get_height(shm_buffer), wl_shm_buffer_get_width(shm_buffer), x, y, wl_shm_buffer_get_format(shm_buffer));
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

void soilleir_quit(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	struct wl_display *display = data;
	wl_display_terminate(display);
}

void soilleir_pointer_motion(void *data, uint32_t mods, int32_t dx, int32_t dy) {
	soilleir_server_t *soilleir = data;
	swl_client_t *client;
	soilleir_xdg_toplevel_t *toplevel, *tmp;
	swl_xdg_surface_t *xdg_surface;
	swl_subsurface_t *subsurface;
	swl_surface_t *surface;
	struct wl_array states;
	wl_array_init(&states);
	soilleir->xpos += dx;
	soilleir->ypos += dy;
	int found = 0;


	soilleir->backend->BACKEND_MOVE_CURSOR(soilleir->backend, soilleir->xpos, soilleir->ypos);
	wl_list_for_each_safe(toplevel, tmp, &soilleir->surfaces, link) {
		xdg_surface = wl_resource_get_user_data(toplevel->swl_toplevel->xdg_surface);
		surface = wl_resource_get_user_data(xdg_surface->wl_surface_res);
		if(soilleir->xpos >= surface->position.x && soilleir->xpos <= surface->position.x + surface->width &&
			 soilleir->ypos >= surface->position.y && soilleir->ypos <= surface->position.y + surface->height) {
			found = 1;
			if(soilleir->active != toplevel) {
				swl_seat_set_focused_surface_keyboard(soilleir->seat, surface->resource);
			}
			
			if(soilleir->pointer_surface != surface->resource) {
				swl_seat_set_focused_surface_pointer(soilleir->seat, surface->resource,
						wl_fixed_from_int(soilleir->xpos - surface->position.x), wl_fixed_from_int(soilleir->ypos - surface->position.y));
			} else {
				/*Dont set the cursor if this is already the client*/
				break;
			}
			wl_list_remove(&toplevel->link);
			/*Make us the new head and thus the last node to be rendered*/
			wl_list_insert(&soilleir->surfaces, &toplevel->link);

			soilleir->active = toplevel;
			soilleir->pointer_surface = surface->resource;
			break;
		}
		wl_list_for_each(subsurface, &surface->subsurfaces, link) {
			if(soilleir->xpos >= surface->position.x + subsurface->position.x &&
					soilleir->xpos <= surface->position.x + subsurface->position.x + subsurface->surface->width &&
					soilleir->ypos >= surface->position.y + subsurface->position.y &&
					soilleir->ypos <= surface->position.y + subsurface->position.y + subsurface->surface->height) {
				if(soilleir->pointer_surface == subsurface->surface->resource) break;
				/*todo keyboard*/
				found = 1;
				swl_seat_set_focused_surface_pointer(soilleir->seat, subsurface->surface->resource,
						wl_fixed_from_int(soilleir->xpos - (surface->position.x + subsurface->position.x)), wl_fixed_from_int(soilleir->ypos - (surface->position.y + subsurface->position.y)));
				soilleir->pointer_surface = subsurface->surface->resource;
			}
		}
	}

	/*Find the client*/
	wl_list_for_each(client, &soilleir->clients, link) {
		if(toplevel->client != client->client) continue;
		if(client->cursor) {
			swl_surface_t *cursor_surface = wl_resource_get_user_data(client->cursor);
			/*the types here don't actually have an effect*/
			soilleir->backend->BACKEND_SET_CURSOR(soilleir->backend, cursor_surface->texture, 24, 24, 0, 0);
		}
	}


	if(!found) {
		soilleir->pointer_surface = NULL;
		swl_seat_set_focused_surface_pointer(soilleir->seat, NULL, 0, 0);
	}

	if(soilleir->pointer_surface == NULL || !soilleir->active) {
		return;
	}

	xdg_surface = wl_resource_get_user_data(soilleir->active->swl_toplevel->xdg_surface);
	surface = wl_resource_get_user_data(xdg_surface->wl_surface_res);

	if(mods == (SWL_MOD_CTRL)) {
		surface->position.y += dy;
		surface->position.x += dx;
	} else if(mods == (SWL_MOD_ALT)) {
		surface->width += dx;
		surface->height += dy;
		
		if(xdg_surface->idle_configure == NULL) {
			xdg_surface->idle_configure = wl_event_loop_add_idle(wl_display_get_event_loop(soilleir->display),
			swl_xdg_surface_send_configure, xdg_surface);
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
	swl_xdg_surface_t *xdg_surface;
	soilleir_xdg_toplevel_t *toplevel;
	bool next = false;

	wl_list_for_each(toplevel, &soilleir->surfaces, link) {
		if(soilleir->active == toplevel) next = true;
		else if(next) {
			soilleir->active = toplevel;
			xdg_surface = wl_resource_get_user_data(soilleir->active->swl_toplevel->xdg_surface);
			swl_seat_set_focused_surface_keyboard(soilleir->seat, xdg_surface->wl_surface_res);
			return;
		}
	}

	/*Grab the first*/
	wl_list_for_each(toplevel, &soilleir->surfaces, link) {
		soilleir->active = toplevel;
		xdg_surface = wl_resource_get_user_data(soilleir->active->swl_toplevel->xdg_surface);
		swl_seat_set_focused_surface_keyboard(soilleir->seat, xdg_surface->wl_surface_res);
		return;
	}
}

void soilleir_spawn(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	if(!state) return;
	if(fork() == 0) {
		execlp(data, data, NULL);
	}
}

void soilleir_kill_client(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state) {
	soilleir_server_t *server = data;

	/*No client*/
	if(!server->active) return;
	xdg_toplevel_send_close(server->active->swl_toplevel->resource);
}

static void swl_surface_render(swl_surface_t *surface, swl_output_t *output) {
	swl_subsurface_t *subsurface;
	if(surface->texture) {
		output->renderer->draw_texture(output->renderer, surface->texture, 
				surface->position.x - output->x, surface->position.y - output->y,
				surface->position.x, surface->position.y, SWL_RENDER_TEXTURE_MODE_NORMAL);
		if(surface->frame) {
			wl_callback_send_done(surface->frame, 0);
			wl_resource_destroy(surface->frame);
			surface->frame = NULL;
		}	
	}

	/*Again same deal here last node acts as the lowest z-axis subsurfaces
	 *and the head acts as the highest
	 */
	wl_list_for_each_reverse(subsurface, &surface->subsurfaces, link) {
		if(subsurface->surface->texture) {
			output->renderer->draw_texture(output->renderer, subsurface->surface->texture,
				(surface->position.x - output->x) + subsurface->position.x, 
				(surface->position.y - output->y) + subsurface->position.y, 
				(surface->position.x) + subsurface->position.x, 
				(surface->position.y) + subsurface->position.y,
				SWL_RENDER_TEXTURE_MODE_NORMAL);
		}
		if(subsurface->surface->frame) {
			wl_callback_send_done(subsurface->surface->frame, 0);
			wl_resource_destroy(subsurface->surface->frame);
			subsurface->surface->frame = NULL;
		}
	}
}

static void soilleir_toplevel_render(soilleir_xdg_toplevel_t *toplevel, swl_output_t *output) {
	swl_xdg_surface_t *xdg_surface, *popup_xdg_surface;
	swl_surface_t *surface, *popup_surface;
	soilleir_xdg_popup_t *popup;

	xdg_surface = wl_resource_get_user_data(toplevel->swl_toplevel->xdg_surface);
	surface = wl_resource_get_user_data(xdg_surface->wl_surface_res);

	swl_surface_render(surface, output);
	wl_list_for_each(popup, &toplevel->popups, link) {
		popup_xdg_surface = wl_resource_get_user_data(popup->popup->xdg_surface);
		popup_surface = wl_resource_get_user_data(popup_xdg_surface->wl_surface_res);
		swl_surface_render(popup_surface, output);
	}
}

static void soilleir_frame(struct wl_listener *listener, void *data) {
	swl_output_t *output = data;
	soilleir_output_t *soil_output = wl_container_of(listener, soil_output, frame_listener);
	soilleir_xdg_toplevel_t *toplevel;
	swl_client_t *client;
	soilleir_server_t *server = soil_output->server;
	output->renderer->attach_target(output->renderer, output->targets[output->front_buffer]);
	output->renderer->begin(output->renderer);

	output->renderer->clear(output->renderer, 0.2f, 0.2f, 0.2f, 1.0f);
	if(output->background) {
		output->renderer->draw_texture(output->renderer, output->background, 0, 0, 0, 0, SWL_RENDER_TEXTURE_MODE_NORMAL);
	}

	/*Last element acts as the lowest Z-axis and is drawn first
	 *and the first(HEAD) acts as the highest Z-Axis and is renderered on top
	 *to ensure it is always the top client
	 */
	wl_list_for_each_reverse(toplevel, &server->surfaces, link) {
		soilleir_toplevel_render(toplevel, output);
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

	soil_output->common->renderer->destroy_texture(soil_output->common->renderer, soil_output->common->background);

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

static void soilleir_toplevel_destroy(struct wl_listener *listener, void *data) {
	soilleir_xdg_toplevel_t *soil_toplevel = wl_container_of(listener, soil_toplevel, destroy);
	swl_xdg_surface_t *surface = wl_resource_get_user_data(soil_toplevel->swl_toplevel->xdg_surface);

	if(soil_toplevel->soilleir->active == soil_toplevel) {
		soil_toplevel->soilleir->active = NULL;
		swl_seat_set_focused_surface_keyboard(soil_toplevel->soilleir->seat, NULL);
	}
	if(soil_toplevel->soilleir->pointer_surface == surface->wl_surface_res) {
		soil_toplevel->soilleir->pointer_surface = NULL;
		swl_seat_set_focused_surface_pointer(soil_toplevel->soilleir->seat, NULL, 0, 0);
	}

	wl_list_remove(&soil_toplevel->destroy.link);
	wl_list_remove(&soil_toplevel->link);
	free(soil_toplevel);
}

static void soilleir_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	swl_xdg_toplevel_t *toplevel = data;
	soilleir_xdg_toplevel_t *soil_toplevel = calloc(1, sizeof(soilleir_xdg_toplevel_t));
	soilleir_xdg_surface_t *surface = wl_container_of(listener, surface, new_toplevel);
	swl_client_t *client = swl_get_client_or_create(wl_resource_get_client(surface->surface->resource), &surface->soilleir->clients);
	soil_toplevel->swl_toplevel = toplevel;
	soil_toplevel->soilleir = surface->soilleir;
	soil_toplevel->client = wl_resource_get_client(toplevel->resource);

	wl_list_insert(&surface->soilleir->surfaces, &soil_toplevel->link);

	soil_toplevel->destroy.notify = soilleir_toplevel_destroy;
	wl_signal_add(&soil_toplevel->swl_toplevel->destroy, &soil_toplevel->destroy);
	wl_list_init(&soil_toplevel->popups);
}

static void soilleir_xdg_popup_destroy(struct wl_listener *listener, void *data) {
	swl_client_t *client;
	soilleir_xdg_toplevel_t *toplevel;
	soilleir_xdg_popup_t *popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->link);

	wl_list_remove(&popup->destroy.link);
}

static void soilleir_new_xdg_popup(struct wl_listener *listener, void *data) {
	soilleir_xdg_popup_t *popup = calloc(1, sizeof(soilleir_xdg_popup_t));
	soilleir_xdg_surface_t *surface = wl_container_of(listener, surface, new_popup);
	swl_xdg_popup_t *swl_popup = data;
	swl_client_t *client;
	soilleir_xdg_toplevel_t *toplevel;

	popup->server = surface->soilleir;
	popup->popup = data;
	popup->destroy.notify = soilleir_xdg_popup_destroy;

	wl_list_for_each(toplevel, &surface->soilleir->surfaces, link) {
		if(toplevel->swl_toplevel->xdg_surface == popup->popup->parent) {
			wl_list_insert(&toplevel->popups, &popup->link);
		}
	}

	wl_signal_add(&popup->popup->destroy, &popup->destroy);
}

static void soilleir_xdg_surface_destroy(struct wl_listener *listener, void *data) {
	soilleir_xdg_surface_t *xdg_surface = wl_container_of(listener, xdg_surface, destroy);
	wl_list_remove(&xdg_surface->new_popup.link);
	wl_list_remove(&xdg_surface->new_toplevel.link);
	wl_list_remove(&xdg_surface->destroy.link);

	free(xdg_surface);
};

static void soilleir_new_xdg_surface(struct wl_listener *listener, void *data) {
	soilleir_xdg_surface_t *xdg_surface = calloc(1, sizeof(soilleir_xdg_surface_t));
	swl_xdg_surface_t *swl_xdg_surface = data;

	xdg_surface->soilleir = wl_container_of(listener, xdg_surface->soilleir, new_xdg_surface);
	xdg_surface->surface = swl_xdg_surface;
	xdg_surface->destroy.notify = soilleir_xdg_surface_destroy;
	xdg_surface->new_toplevel.notify = soilleir_new_xdg_toplevel;
	xdg_surface->new_popup.notify = soilleir_new_xdg_popup;
	wl_signal_add(&swl_xdg_surface->new_toplevel, &xdg_surface->new_toplevel);
	wl_signal_add(&swl_xdg_surface->destroy, &xdg_surface->destroy);
	wl_signal_add(&swl_xdg_surface->new_popup, &xdg_surface->new_popup);
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
	if(surface_res == NULL) {
		return;
	}
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);


	surface->role = &swl_cursor_surface_role;
	surface->role_resource = pointer;

	swl_client_t *client = swl_get_client_or_create(wl_resource_get_client(surface_res), &server->clients);
	client->cursor = surface_res;
}


void soilleir_surface_destroy(struct wl_listener *listener, void *data) {
	soilleir_surface_t *surface = wl_container_of(listener, surface, destroy);

	if(surface->output && surface->swl_surface->texture) {
		surface->output->common->renderer->destroy_texture(surface->output->common->renderer, surface->swl_surface->texture);
	}

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

	swl_log_set_level(SWL_LOG_DEBUG);

	/*Skip Program name*/
	for(int arg = 1; arg < argc; ++arg) {
		if(strcmp("--log-file", argv[arg]) == 0) {
			if(argv[arg+1] == NULL) {
				printf("--log_file used without an argument");
				return 1;
			}

			if(strcmp(argv[arg+1], "stdout") == 0) {
				swl_log_set_fp(stdout);
			} else if(strcmp(argv[arg+1], "stderr") == 0) {
				swl_log_set_fp(stderr);
			} else {
				swl_log_open(argv[arg+1]);
			}
			arg++;
		} else {
			printf("Ignoring unknown positional argument: %s\n", argv[arg]);
		}
	}

	soilleir.display = wl_display_create();
	setenv("WAYLAND_DISPLAY", wl_display_add_socket_auto(soilleir.display), 1);
	soilleir_ipc_init(&soilleir);

	wl_list_init(&soilleir.outputs);

	soilleir.base = swl_xdg_wm_base_create(soilleir.display, NULL);
	soilleir.new_xdg_surface.notify = soilleir_new_xdg_surface;
	wl_signal_add(&soilleir.base->new_surface, &soilleir.new_xdg_surface);

	wl_display_init_shm(soilleir.display);
	wl_global_create(soilleir.display, &zswl_screenshot_manager_interface, 1, NULL, zswl_screenshot_manager_bind);

	soilleir.backend = swl_backend_create_by_env(soilleir.display);
	soilleir.compositor = swl_compositor_create(soilleir.display, &soilleir);
	soilleir.new_surface.notify = soilleir_new_surface;
	wl_signal_add(&soilleir.compositor->new_surface, &soilleir.new_surface);
	soilleir.subcompositor = swl_subcompositor_create(soilleir.display);

	soilleir.seat = swl_seat_create(soilleir.display, soilleir.backend, "seat0", kmap);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_ALT, XKB_KEY_Escape, soilleir_quit, soilleir.display);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_ALT, XKB_KEY_Return, soilleir_spawn, "foot");
	swl_seat_add_binding(soilleir.seat, SWL_MOD_ALT, XKB_KEY_Tab, soilleir_next_client, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_1, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_2, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_3, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_4, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_5, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_CTRL | SWL_MOD_ALT, XKB_KEY_XF86Switch_VT_6, soilleir_switch_session, &soilleir);
	swl_seat_add_binding(soilleir.seat, SWL_MOD_ALT, XKB_KEY_q, soilleir_kill_client, &soilleir);

	swl_seat_add_pointer_callback(soilleir.seat, soilleir_pointer_motion, &soilleir);
	swl_seat_add_set_cursor_callback(soilleir.seat, soilleir_set_cursor_callback, &soilleir);

	wl_list_init(&soilleir.clients);
	wl_list_init(&soilleir.surfaces);

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
