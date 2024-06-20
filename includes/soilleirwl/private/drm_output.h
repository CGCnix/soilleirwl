#pragma once


#include <soilleirwl/interfaces/swl_output.h>

#include <stdint.h>
#include <xf86drmMode.h>
#include <gbm.h>

typedef struct swl_buffer {
	uint32_t handle, fb_id;
	uint32_t format, height, width, pitch;
	size_t size, offset;
	uint64_t modifiers;

	uint8_t *data;
} swl_buffer_t;

typedef struct swl_drm_output {
	swl_output_t common;

	drmModeCrtcPtr original_crtc;
	drmModeConnectorPtr connector;

	swl_buffer_t buffer[2];
	int front_buffer;

	int pending;
	int shutdown;

	struct wl_list link;
	int drm_fd;
} swl_drm_output_t;


