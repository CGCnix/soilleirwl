#pragma once

#include "./ipc.h"

#include <soilleirwl/interfaces/swl_compositor.h>
#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_seat.h>
#include <soilleirwl/backend/backend.h>
#include <wayland-server-core.h>

#include <soilleirwl/interfaces/swl_xdg_shell.h>
#include <wayland-util.h>

typedef struct soilleir_xdg_toplevel soilleir_xdg_toplevel_t;


typedef struct {
	struct wl_display *display;
	swl_backend_t *backend;

	swl_compositor_t *compositor;
	swl_subcompositor_t *subcompositor;
	swl_xdg_wm_base_t *base;

	int32_t xpos;
	int32_t ypos;

	swl_seat_t *seat;

	soilleir_xdg_toplevel_t *active;
	struct wl_resource *pointer_surface;

	struct wl_listener output_listner;
	struct wl_list clients;
	struct wl_list surfaces;

	void *bg;

	struct wl_list outputs;

	server_ipc_sock ipc;
	struct wl_listener new_surface;
	struct wl_listener new_xdg_surface;
} soilleir_server_t;

typedef struct {
	swl_xdg_popup_t *popup;
	soilleir_server_t *server;
	struct wl_listener destroy;
	struct wl_list link;
} soilleir_xdg_popup_t;

struct soilleir_xdg_toplevel {
	swl_xdg_toplevel_t *swl_toplevel;
	struct wl_client *client;
	soilleir_server_t *soilleir;
	struct wl_list popups;

	struct wl_listener destroy;
	struct wl_list link;
};

typedef struct {
	soilleir_server_t *server;
	swl_output_t *common;

	struct wl_listener destroy;
	struct wl_listener frame_listener;
	struct wl_listener bind;
	struct wl_list link;
} soilleir_output_t;

typedef struct swl_client {
	struct wl_client *client;
	struct wl_listener destroy;

	struct wl_resource *cursor;
	struct wl_resource *output;
	struct wl_list link;
} swl_client_t;
