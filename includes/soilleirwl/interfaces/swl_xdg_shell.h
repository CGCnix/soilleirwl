#pragma once

#include <private/xdg-shell-server.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <soilleirwl/interfaces/swl_surface.h>

#define SWL_XDG_WM_BASE_VERSION 6
#define SWL_XDG_SURFACE_VERSION 6
#define SWL_XDG_TOPLEVEL_VERSION 6
#define SWL_XDG_POPUP_VERSION 6
#define SWL_XDG_POSITIONER_VERSION 6

typedef struct swl_anchor {
	int32_t x, y;
	int32_t width, height;
	uint32_t type;
} swl_anchor_t;

typedef struct swl_positioner_offset {
	int32_t x, y;
} swl_positioner_offset_t;

typedef struct swl_positioner_size {
	int32_t width, height;
} swl_positioner_size_t;

typedef struct swl_xdg_positioner {
	struct wl_resource *resource;
	swl_positioner_offset_t offset;
	swl_anchor_t anchor;
	swl_positioner_size_t size;
	swl_positioner_size_t parent;
	uint32_t gravity;
	struct wl_signal destroy;
} swl_xdg_positioner_t;

typedef struct swl_xdg_toplevel {
	struct wl_resource *resource;
	struct wl_resource *xdg_surface;
	const char *title;
	const char *app_id;

	struct wl_signal destroy;
} swl_xdg_toplevel_t;

typedef struct swl_xdg_popup {
	struct wl_resource *resource;
	struct wl_resource *xdg_surface;
	struct wl_resource *parent;

	swl_positioner_size_t size;
	swl_positioner_offset_t offset;
	swl_anchor_t anchor;
	uint32_t gravity;

	bool repos;
	uint32_t token;

	struct wl_signal destroy;
} swl_xdg_popup_t;

typedef struct swl_xdg_surface swl_xdg_surface_t;

typedef struct swl_xdg_surface_role {
	void (*configure)(struct wl_client *client, swl_xdg_surface_t *surface, struct wl_resource *role);
} swl_xdg_surface_role_t;

typedef struct swl_xdg_surface_geometry {
	int x, y;
	int width, height;
} swl_xdg_surface_geometry_t;

struct swl_xdg_surface {
	struct wl_resource *resource;

	struct wl_event_source *idle_configure;
	struct wl_signal new_toplevel;
	struct wl_signal new_popup;
	struct wl_signal destroy;

	swl_xdg_surface_geometry_t geometry;

	struct wl_resource *wl_surface_res;
	struct wl_resource *role_resource;
	swl_xdg_surface_role_t *role;
};

typedef struct swl_xdg_wm_base {
	struct wl_global *global;
	struct wl_signal new_surface;
	struct wl_signal new_positioner;
} swl_xdg_wm_base_t;

void swl_xdg_surface_send_configure(void *data);
void swl_xdg_wm_base_destroy(swl_xdg_wm_base_t *xdg_base);
swl_xdg_wm_base_t *swl_xdg_wm_base_create(struct wl_display *display, void *data);
