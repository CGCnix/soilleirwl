#pragma once


#include <soilleirwl/interfaces/swl_output.h>

#include <stdint.h>
#include <xf86drmMode.h>
#include <gbm.h>

typedef struct swl_drm_output {
	swl_output_t common;

	drmModeCrtcPtr original_crtc;
	drmModeConnectorPtr connector;

	int pending;
	int shutdown;

	struct wl_list link;
	int drm_fd;
	struct gbm_device *gbm;
} swl_drm_output_t;
