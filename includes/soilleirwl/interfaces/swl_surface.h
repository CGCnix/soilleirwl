#pragma once
/* Surface needs a pending and a current state
 * pending { pending buffer, 
 * 						buffers x & y, damages, 
 * 						opaque_region, callback_frame, 
 * 						input_region, buffer_transform, 
 * 						buffer_scale 
 * 				 }
 * current { current buffer, 
 * 						buffers x & y, damages, 
 * 						opaque_region, input_region, 
 * 						buffer_transform, buffer_scale 
 * 				 }
 *
 */

#include <stdint.h>
#include <stdbool.h>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-server.h>

#include <soilleirwl/renderer.h>

#define SWL_SURFACE_PENDING_NONE 0
#define SWL_SURFACE_PENDING_BUFFER (1)
#define SWL_SURFACE_PENDING_DAMAGE (1 << 1)
#define SWL_SURFACE_PENDING_OPAQUE (1 << 2)
#define SWL_SURFACE_PENDING_INPUT (1 << 3)
#define SWL_SURFACE_PENDING_TRANSFORM (1 << 4)
#define SWL_SURFACE_PENDING_SCALE (1 << 5)
/*IDEA: Do I need a seperate pending and damage type for damage buffers?*/
typedef struct swl_surface swl_surface_t;

typedef struct swl_surface_role {
	void (*precommit)(struct wl_client *client, swl_surface_t *surface, struct wl_resource *role);
	void (*postcommit)(struct wl_client *client, swl_surface_t *surface, struct wl_resource *role);
} swl_surface_role_t;

/*TODO: this doesn't represent all possible buffer types*/
typedef struct swl_surface_buffer {
	struct wl_resource *buffer;
	int32_t x, y;
} swl_surface_buffer_t;

typedef struct swl_rect {
	int32_t x, y;
	int32_t width, height;
}	swl_rect_t;

typedef struct swl_surface_pos {
	int32_t x, y;
	int32_t z;
} swl_surface_pos_t;

typedef struct swl_surface_state {
	int32_t scale;
	int32_t transform;
	struct wl_resource *input_region;
	struct wl_resource *opaque_region;

	swl_rect_t damage;
	swl_surface_buffer_t buffer;
	int32_t width, height;
} swl_surface_state_t;

typedef struct swl_surface {
	struct wl_resource *resource;
	struct wl_resource *compositor;
	struct wl_resource *frame;
	bool configured;

	swl_surface_pos_t position;
	swl_rect_t extent;

	/*Pending State Swaped into current on commit*/
	uint32_t pending_changes; /*<Pending changes to apply on commit*/
	swl_surface_state_t pending;
	/*Current*/

	uint32_t scale;
	uint32_t transform;

	struct wl_resource *input_region;
	struct wl_resource *opaque_region;

	swl_surface_buffer_t buffer;
	int32_t width, height;
	int32_t x, y;

	struct wl_list subsurfaces;
	swl_texture_t *texture;

	struct wl_resource *role_resource;
	swl_surface_role_t *role;
	struct wl_signal commit;
	struct wl_signal destroy;
} swl_surface_t;

typedef struct swl_subsurface {
	struct wl_resource *resource;
	swl_surface_t *surface; /*The Surface Connected to this subsurface*/
	swl_surface_t *parent;

	swl_surface_pos_t position;
	struct wl_list link;
	bool sync;
	/*TODO Place below/above*/
} swl_subsurface_t;
