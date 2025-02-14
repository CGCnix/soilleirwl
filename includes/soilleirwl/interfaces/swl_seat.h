#pragma once

#include <stdint.h>

#include <wayland-util.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

#include <soilleirwl/backend/backend.h>

#define SWL_MOD_CTRL 4
#define SWL_MOD_ALT 8

typedef struct swl_seat swl_seat_t;

int swl_seat_add_binding(swl_seat_t *seat, xkb_mod_mask_t mods, xkb_keysym_t key,  void (*callback)(void *data, xkb_mod_mask_t mods, xkb_keysym_t sym, uint32_t state), void *data);
void swl_seat_set_keymap(swl_seat_t *seat, char *map);
swl_seat_t *swl_seat_create(struct wl_display *display, swl_backend_t *backend, const char *name, const char *kmap);

void swl_seat_set_focused_surface_keyboard(swl_seat_t *seat, struct wl_resource *resource);
void swl_seat_set_focused_surface_pointer(swl_seat_t *seat, struct wl_resource *resource, wl_fixed_t x, wl_fixed_t y);
int swl_seat_add_pointer_callback(swl_seat_t *seat, void (*callback)(void *data, uint32_t mods, int32_t dx, int32_t dy), void *data);
void swl_seat_add_set_cursor_callback(swl_seat_t *seat, void (*callback)(void *data, struct wl_resource *pointer, struct wl_resource *surface, int32_t dx, int32_t dy), void *data);

void swl_seat_destroy(swl_seat_t *seat);
