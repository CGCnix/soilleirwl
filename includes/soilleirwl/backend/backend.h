#pragma once

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <soilleirwl/renderer.h>

typedef struct swl_backend swl_backend_t;

struct swl_backend {
	int (*BACKEND_START)(swl_backend_t *backend);
	int (*BACKEND_STOP)(swl_backend_t *backend);
	swl_renderer_t *(*BACKEND_GET_RENDERER)(swl_backend_t *backend);
	void (*BACKEND_DESTROY)(swl_backend_t *backend);
	void (*BACKEND_ADD_NEW_OUTPUT_LISTENER)(swl_backend_t *backend, struct wl_listener *listener);
	void (*BACKEND_ADD_NEW_INPUT_LISTENER)(swl_backend_t *backend, struct wl_listener *listener);
	void (*BACKEND_ADD_ACTIVATE_LISTENER)(swl_backend_t *backend, struct wl_listener *listener);
	void (*BACKEND_ADD_DISABLE_LISTENER)(swl_backend_t *backend, struct wl_listener *listener);
};


swl_backend_t *swl_backend_create_by_env(struct wl_display *display);
