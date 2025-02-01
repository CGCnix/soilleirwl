#pragma once

#include <soilleirwl/renderer.h>
#include <wayland-server.h>

typedef struct {
	/*WlGlobal*/
	struct wl_global *wl_compositor;

	/*Allow a Compositor of new client popup or surface*/
	struct wl_signal xdg_popup, xdg_surface, wl_surface, wl_region;
} swl_compositor_t;


typedef struct {
	/*WlGlobal*/
	struct wl_global *wl_subcompositor;

	/*Allow a Compositor of new client popup or surface*/
	struct wl_signal wl_subsurface;
} swl_subcompositor_t;

/*TODO/HACK: this only works on one GPU atm this is fine for now but LATER FIXME!!!*/
swl_compositor_t *swl_compositor_create(struct wl_display *display, swl_renderer_t *render);
void swl_compositor_destroy(swl_compositor_t *compositor);

swl_subcompositor_t *swl_subcompositor_create(struct wl_display *display);
void swl_subcompositor_destroy(swl_subcompositor_t *subcompositor);
