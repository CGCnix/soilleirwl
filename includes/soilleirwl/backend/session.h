#pragma once

#include <wayland-server-core.h>

typedef struct swl_session_backend swl_session_backend_t;

typedef int (*SWL_SEAT_OPEN_DEV)(swl_session_backend_t *session, const char *path, int *fd);
typedef int (*SWL_SEAT_CLOSE_DEV)(swl_session_backend_t *session, int dev);
typedef int (*SWL_SEAT_SWITCH_VT)(swl_session_backend_t *session, int vt);
typedef int (*SWL_SESSION_BACKEND_START)(swl_session_backend_t *session);
typedef int (*SWL_SESSION_BACKEND_STOP)(swl_session_backend_t *session);
typedef void (*SWL_SESSION_BACKEND_DESTROY)(swl_session_backend_t *session);

struct swl_session_backend {
	SWL_SESSION_BACKEND_START start;
	SWL_SESSION_BACKEND_STOP stop;
	SWL_SESSION_BACKEND_DESTROY destroy;
	SWL_SEAT_OPEN_DEV open_dev;
	SWL_SEAT_CLOSE_DEV close_dev;
	SWL_SEAT_SWITCH_VT switch_vt;

	struct wl_signal activate;
	struct wl_signal disable;
};


swl_session_backend_t *swl_libseat_backend_create(struct wl_display *display);
