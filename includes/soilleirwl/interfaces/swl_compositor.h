#pragma once

#include <soilleirwl/renderer.h>
#include <wayland-server.h>

typedef struct {
	/*WlGlobal*/
	struct wl_global *wl_compositor;

	void *data;
	/*Allow a Compositor to be alerted of a new region or surface*/
	struct wl_signal new_surface, new_region;
} swl_compositor_t;


typedef struct {
	/*WlGlobal*/
	struct wl_global *wl_subcompositor;

	/*Allow a Compositor of new subsurface*/
	struct wl_signal wl_subsurface;
} swl_subcompositor_t;

swl_compositor_t *swl_compositor_create(struct wl_display *display, void *data);
void swl_compositor_destroy(swl_compositor_t *compositor);

swl_subcompositor_t *swl_subcompositor_create(struct wl_display *display);
void swl_subcompositor_destroy(swl_subcompositor_t *subcompositor);
