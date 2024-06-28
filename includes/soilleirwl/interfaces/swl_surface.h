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

#include "soilleirwl/renderer.h"
#include <wayland-server.h>

#include <stdint.h>

#define SWL_SURFACE_PENDING_NONE 0
#define SWL_SURFACE_PENDING_BUFFER (1)
#define SWL_SURFACE_PENDING_DAMAGE (1 << 1)
#define SWL_SURFACE_PENDING_OPAQUE (1 << 2)
#define SWL_SURFACE_PENDING_INPUT (1 << 3)
#define SWL_SURFACE_PENDING_TRANSFORM (1 << 4)
#define SWL_SURFACE_PENDING_SCALE (1 << 5)
/*IDEA: Do I need a seperate pending and damage type for damage buffers?*/

/*TODO: this doesn't represent all possible buffer types*/
typedef struct swl_surface_buffer {
	void *data;
	int32_t x, y;
	int32_t width, height;
	int32_t stride, size;
} swl_surface_buffer_t;

typedef struct swl_rect {
	int32_t x, y;
	int32_t width, height;

	struct wl_list *link;
}	swl_rect_t;

typedef struct swl_surface_state {
	int32_t scale;
	int32_t transform;
	struct wl_resource *input_region;
	struct wl_resource *opaque_region;

	struct wl_list damages;
	struct wl_resource *buffer;
} swl_surface_state_t;

typedef struct swl_surface {
	struct wl_resource *resource;
	struct wl_resource *frame;
	
	/*Pending State Swaped into current on commit*/
	uint32_t pending_changes; /*<Pending changes to apply on commit*/
	swl_surface_state_t pending;

	/*Current Surface*/
	int32_t scale;
	int32_t transform;
	struct wl_resource *input_region;
	struct wl_resource *opaque_region;
	
	/*HACK:*/
	swl_renderer_t *renderer;

	/*todo some kind of buffer*/
	swl_texture_t *texture;


	int32_t x, y;
	/*Resource with the associate role*/
	struct wl_resource *role;
} swl_surface_t;
