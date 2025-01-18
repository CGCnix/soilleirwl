#pragma once

#include <soilleirwl/backend/backend.h>
#include <wayland-server-core.h>

swl_backend_t *swl_tty_backend_create(struct wl_display *display);
