#pragma once

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include "xdg-shell.h"

typedef struct {
	struct wl_display *display;
	struct wl_compositor *compistor;
	struct wl_shm *shm;
	struct wl_subcompositor *subcompistor;
	struct xdg_wm_base *base;

	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	struct wl_seat *seat;
} client_t;
