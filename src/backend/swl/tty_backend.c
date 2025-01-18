#include "soilleirwl/renderer.h"
#include <soilleirwl/backend/hotplug.h>
#include <soilleirwl/backend/display.h>
#include <soilleirwl/backend/input.h>
#include <soilleirwl/backend/session.h>
#include <soilleirwl/backend/backend.h>
#include <stdlib.h>
#include <wayland-server-core.h>

typedef struct swl_tty_backend {
	swl_backend_t backend;

	swl_display_backend_t *display;
	swl_input_backend_t *input;
	swl_session_backend_t *session;
	swl_hotplug_backend_t *hotplug;
} swl_tty_backend_t;

#define swl_backend_to_tty_backend(ptr) ((swl_tty_backend_t*)ptr);

int swl_tty_backend_start(swl_backend_t *backend) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	tty->display->SWL_DISPLAY_BACKEND_START(tty->display);
	tty->hotplug->start(tty->hotplug);
	tty->input->SWL_INPUT_BACKEND_START(tty->input);
	tty->session->start(tty->session);

	return 0;
}

int swl_tty_backend_stop(swl_backend_t *backend) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	tty->session->stop(tty->session);
	tty->input->SWL_INPUT_BACKEND_STOP(tty->input);
	tty->hotplug->stop(tty->hotplug);
	tty->display->SWL_DISPLAY_BACKEND_STOP(tty->display);

	return 0;
}

void swl_tty_backend_destroy(swl_backend_t *backend) {
	/*TODO*/
}

void swl_tty_backend_add_new_output_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	wl_signal_add(&tty->display->new_output, listener);
}

void swl_tty_backend_add_new_input_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	wl_signal_add(&tty->input->new_input, listener);
}

void swl_tty_backend_add_new_activate_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	wl_signal_add(&tty->session->activate, listener);
}

void swl_tty_backend_add_new_disable_listener(swl_backend_t *backend, struct wl_listener *listener) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	wl_signal_add(&tty->session->disable, listener);
}

swl_renderer_t *swl_tty_backend_get_renderer(swl_backend_t *backend) {
	swl_tty_backend_t *tty = swl_backend_to_tty_backend(backend);

	return tty->display->SWL_DISPLAY_BACKEND_GET_RENDERER(tty->display);
}



swl_backend_t *swl_tty_backend_create(struct wl_display *display) {
	swl_tty_backend_t *tty = calloc(1, sizeof(swl_tty_backend_t));
	const char *drm_dev = "/dev/dri/card0";

	if(getenv("SWL_DRM_DEV")) {
		drm_dev = getenv("SWL_DRM_DEV");
	}

	tty->session = swl_libseat_backend_create(display);
	tty->hotplug = swl_libudev_backend_create(display);
	tty->input = swl_libinput_backend_create(display, tty->session, tty->hotplug);
	tty->display = swl_drm_create_backend(display, tty->session, drm_dev);
	
	tty->backend.BACKEND_GET_RENDERER = swl_tty_backend_get_renderer;
	tty->backend.BACKEND_DESTROY = swl_tty_backend_destroy;
	tty->backend.BACKEND_START = swl_tty_backend_start;
	tty->backend.BACKEND_STOP = swl_tty_backend_stop;
	tty->backend.BACKEND_ADD_NEW_OUTPUT_LISTENER = swl_tty_backend_add_new_output_listener;
	tty->backend.BACKEND_ADD_NEW_INPUT_LISTENER = swl_tty_backend_add_new_input_listener;
	tty->backend.BACKEND_ADD_ACTIVATE_LISTENER = swl_tty_backend_add_new_activate_listener;
	tty->backend.BACKEND_ADD_DISABLE_LISTENER = swl_tty_backend_add_new_disable_listener;
	return (swl_backend_t*)tty;
}
