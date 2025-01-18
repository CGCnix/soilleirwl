#pragma once

#include <stdint.h>
#include <gbm.h>

typedef struct swl_gbm_buffer {
	struct gbm_bo *bo;
	struct gbm_device *device;
	int render;
	uint32_t handle;

	uint32_t height, width;
	uint64_t pitch, size;
	uint32_t bpp;
} swl_gbm_buffer_t;

swl_gbm_buffer_t *swl_gbm_buffer_create(struct gbm_device *device, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, uint32_t bpp);
void swl_gbm_buffer_destroy(swl_gbm_buffer_t *gbm);
