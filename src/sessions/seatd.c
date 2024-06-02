#include <soilleirwl/session.h>

#include <stdlib.h>
#include <libseat.h>
#include <wayland-server.h>

typedef struct {
	swl_session_backend_t session;
	struct libseat *libseat;
	char active;

	struct wl_display *display;
	struct wl_event_source *readable;
} swl_seatd_backend_t;

static void swl_seatd_enable(struct libseat *seat, void *data) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)data;

	seatd->active = 1;
	wl_signal_emit(&seatd->session.activate, NULL);
}

static void swl_seatd_disable(struct libseat *seat, void *data) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)data;

	seatd->active = 0;
	wl_signal_emit(&seatd->session.disable, NULL);
	libseat_disable_seat(seatd->libseat);
}

static const struct libseat_seat_listener swl_seat_listener = {
	.enable_seat = swl_seatd_enable,
	.disable_seat = swl_seatd_disable,
};

static int swl_seatd_close_dev(swl_session_backend_t *session, int dev) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)session;
	return libseat_close_device(seatd->libseat, dev);
}

static int swl_seatd_open_dev(swl_session_backend_t *session, const char *path, int *fd) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)session;

	return libseat_open_device(seatd->libseat, path, fd);
}

static int swl_seatd_switch_vt(swl_session_backend_t *session, int vt) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)session;

	return libseat_switch_session(seatd->libseat, vt);
}

static int swl_seatd_readable(int fd, uint32_t mask, void *data) {
	swl_seatd_backend_t *seatd = (swl_seatd_backend_t*)data;
	if(libseat_dispatch(seatd->libseat, 0) < 0) {
		wl_display_terminate(seatd->display);
	}

	return 0;
}


swl_session_backend_t *swl_seatd_backend_create(struct wl_display *display) {
	swl_seatd_backend_t *seatd;
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(display);

	if(!display && !loop) {
		return NULL;
	}

	seatd = calloc(1, sizeof(swl_seatd_backend_t));
	if(!seatd) {
		return NULL;
	}
	wl_signal_init(&seatd->session.disable);	
	wl_signal_init(&seatd->session.activate);
	
	seatd->libseat = libseat_open_seat(&swl_seat_listener, seatd);
	if(!seatd->libseat) {
		free(seatd);
		return NULL;
	}

	while (seatd->active == 0) {
		if(libseat_dispatch(seatd->libseat, -1) == -1) {
			libseat_close_seat(seatd->libseat);
			free(seatd);
			return NULL;
		}
	}
	seatd->readable = wl_event_loop_add_fd(loop, libseat_get_fd(seatd->libseat), 
			WL_EVENT_READABLE, swl_seatd_readable, seatd);
	if(!seatd->readable) {
		libseat_close_seat(seatd->libseat);
		free(seatd);
		return NULL;
	}

	seatd->display = display;
	/*Init common call points*/
	seatd->session.open_dev = swl_seatd_open_dev;
	seatd->session.close_dev = swl_seatd_close_dev;
	seatd->session.switch_vt = swl_seatd_switch_vt;

	return (swl_session_backend_t*)seatd;
}
