#pragma once

#define SWL_MOD_CTRL 4
#define SWL_MOD_ALT 8

#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

#include <soilleirwl/backend/backend.h>

typedef struct swl_seat swl_seat_t;

void swl_seat_set_focused_surface(swl_seat_t *seat, struct wl_resource *resource);
int swl_seat_add_binding(swl_seat_t *seat, xkb_mod_mask_t mods, xkb_keysym_t key,  void (*callback)(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state), void *data);
void swl_seat_set_keymap(swl_seat_t *seat, char *map);
swl_seat_t *swl_seat_create(struct wl_display *display, swl_backend_t *backend, const char *name, const char *kmap);
