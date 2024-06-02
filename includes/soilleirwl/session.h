#pragma once

#include <wayland-server-core.h>


typedef struct swl_session_backend swl_session_backend_t;

typedef int (*SWL_SEAT_OPEN_DEV)(swl_session_backend_t *session, const char *path, int *fd);
typedef int (*SWL_SEAT_CLOSE_DEV)(swl_session_backend_t *session, int dev);
typedef int (*SWL_SEAT_SWITCH_VT)(swl_session_backend_t *session, int vt);

struct swl_session_backend {
	SWL_SEAT_OPEN_DEV open_dev;
	SWL_SEAT_CLOSE_DEV close_dev;
	SWL_SEAT_SWITCH_VT switch_vt;

	struct wl_signal activate;
	struct wl_signal disable;
};

swl_session_backend_t *swl_seatd_backend_create(struct wl_display *display);	
