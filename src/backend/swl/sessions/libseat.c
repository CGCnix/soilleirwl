#include <soilleirwl/backend/session.h>

#include <stdlib.h>
#include <libseat.h>
#include <wayland-server.h>

typedef struct {
	swl_session_backend_t session;
	struct libseat *libseat;
	char active;

	struct wl_display *display;
	struct wl_event_source *readable;
} swl_libseat_backend_t;

static void swl_libseat_enable(struct libseat *seat, void *data) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)data;

	libseat->active = 1;
	wl_signal_emit(&libseat->session.activate, NULL);
}

static void swl_libseat_disable(struct libseat *seat, void *data) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)data;

	libseat->active = 0;
	wl_signal_emit(&libseat->session.disable, NULL);
	libseat_disable_seat(libseat->libseat);
}

static const struct libseat_seat_listener swl_seat_listener = {
	.enable_seat = swl_libseat_enable,
	.disable_seat = swl_libseat_disable,
};

static int swl_libseat_close_dev(swl_session_backend_t *session, int dev) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)session;
	return libseat_close_device(libseat->libseat, dev);
}

static int swl_libseat_open_dev(swl_session_backend_t *session, const char *path, int *fd) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)session;

	return libseat_open_device(libseat->libseat, path, fd);
}

static int swl_libseat_switch_vt(swl_session_backend_t *session, int vt) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)session;

	return libseat_switch_session(libseat->libseat, vt);
}

static int swl_libseat_readable(int fd, uint32_t mask, void *data) {
	swl_libseat_backend_t *libseat = (swl_libseat_backend_t*)data;
	if(libseat_dispatch(libseat->libseat, 0) < 0) {
		wl_display_terminate(libseat->display);
	}

	return 0;
}

void swl_libseat_backend_destroy(swl_session_backend_t *session) {
	swl_libseat_backend_t *libseat;

	libseat = (swl_libseat_backend_t*)session;

	wl_event_source_remove(libseat->readable);
	libseat_close_seat(libseat->libseat);
	free(libseat);
}

int swl_libseat_backend_stop(swl_session_backend_t *session) {
	return 0;
}

int swl_libseat_backend_start(swl_session_backend_t *session) {
	return 0;
}

swl_session_backend_t *swl_libseat_backend_create(struct wl_display *display) {
	swl_libseat_backend_t *libseat;
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(display);

	if(!display && !loop) {
		return NULL;
	}

	libseat = calloc(1, sizeof(swl_libseat_backend_t));
	if(!libseat) {
		return NULL;
	}
	wl_signal_init(&libseat->session.disable);	
	wl_signal_init(&libseat->session.activate);
	
	libseat->libseat = libseat_open_seat(&swl_seat_listener, libseat);
	if(!libseat->libseat) {
		free(libseat);
		return NULL;
	}

	while (libseat->active == 0) {
		if(libseat_dispatch(libseat->libseat, -1) == -1) {
			libseat_close_seat(libseat->libseat);
			free(libseat);
			return NULL;
		}
	}
	libseat->readable = wl_event_loop_add_fd(loop, libseat_get_fd(libseat->libseat), 
			WL_EVENT_READABLE, swl_libseat_readable, libseat);
	if(!libseat->readable) {
		libseat_close_seat(libseat->libseat);
		free(libseat);
		return NULL;
	}

	libseat->display = display;
	/*Init common call points*/
	libseat->session.open_dev = swl_libseat_open_dev;
	libseat->session.close_dev = swl_libseat_close_dev;
	libseat->session.switch_vt = swl_libseat_switch_vt;
	libseat->session.destroy = swl_libseat_backend_destroy;
	libseat->session.stop = swl_libseat_backend_stop;
	libseat->session.start = swl_libseat_backend_start;
	return (swl_session_backend_t*)libseat;
}
