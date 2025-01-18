#pragma once


typedef struct swl_output swl_output_t;

#include <soilleirwl/allocator/gbm.h>
#include <soilleirwl/renderer.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <stdint.h>

#define SWL_OUTPUT_VERSION 4

typedef struct {
	enum wl_output_mode flags;
	int32_t width, height, refresh;
} swl_output_mode_t;

typedef struct {
	uint32_t height, width, pitch;
	uint64_t size;
	void *data;
} swl_output_texture_t;

typedef struct swl_buffer {
	uint32_t handle, fb_id;
	int render; 

	uint32_t format, height, width, pitch;
	size_t size, offset;
	uint64_t modifiers;

	void *map_data;
	uint8_t *data;
} swl_buffer_t;

struct swl_output {
	/*Version 1.0*/
	struct wl_global *global;
	int32_t x, y;
	int32_t width, height;

	char *make;
	char *model;

	enum wl_output_subpixel subpixel;
	enum wl_output_transform tranform;

	swl_output_mode_t mode; /*Current Mode*/

	/*Version 2.0*/
	int32_t scale;

	/*Version 4.0*/
	char *name;
	char *description;

	swl_renderer_t *renderer;

	void (*copy)(swl_output_t *output, struct wl_shm_buffer *buffer, int32_t width, int32_t height, int32_t xoff, int32_t yoff);

	swl_texture_t *background;

	swl_gbm_buffer_t **buffer;
	int front_buffer;

	struct wl_signal frame; /*Inform the compositor this output wants a frame*/
	struct wl_signal destroy;
};
