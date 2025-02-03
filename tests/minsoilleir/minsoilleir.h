#pragma once

#include "./ipc.h"

#include <soilleirwl/interfaces/swl_compositor.h>
#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_seat.h>
#include <soilleirwl/backend/backend.h>
#include <wayland-server-core.h>

typedef struct swl_xdg_toplevel swl_xdg_toplevel_t;

typedef struct {
	struct wl_display *display;
	swl_backend_t *backend;

	swl_compositor_t *compositor;
	swl_subcompositor_t *subcompositor;

	int32_t xpos;
	int32_t ypos;

	swl_seat_t *seat;

	swl_xdg_toplevel_t *active;
	swl_xdg_toplevel_t *pointer_surface;

	struct wl_listener output_listner;
	struct wl_list clients;

	void *bg;

	struct wl_list outputs;

	server_ipc_sock ipc;
	struct wl_listener new_surface;
} soilleir_server_t;

typedef struct {
	soilleir_server_t *server;
	swl_output_t *common;

	struct wl_listener destroy;
	struct wl_listener frame_listener;
	struct wl_listener bind;
	struct wl_list link;
} soilleir_output_t;

typedef struct swl_client {
	struct wl_list surfaces;
	struct wl_client *client;
	struct wl_listener destroy;
	
	struct wl_resource *cursor;
	struct wl_resource *output;
	struct wl_list link;
} swl_client_t;

typedef struct swl_xdg_surface swl_xdg_surface_t;

typedef struct swl_xdg_surface_role {
	void (*configure)(struct wl_client *client, swl_xdg_surface_t *surface, struct wl_resource *role);
} swl_xdg_surface_role_t;

struct swl_xdg_surface {
	swl_surface_t *swl_surface;

	struct wl_event_source *idle_configure;
	soilleir_server_t *backend;
	struct wl_resource *role_resource;
	swl_xdg_surface_role_t *role;
};

typedef struct swl_xdg_toplevel {
	swl_client_t *client;
	const char *title;
	swl_xdg_surface_t *swl_xdg_surface;
	soilleir_server_t *backend;
	struct wl_list link;
} swl_xdg_toplevel_t;


