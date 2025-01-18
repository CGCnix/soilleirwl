#include <gbm.h>
#include <soilleirwl/allocator/gbm.h>
#include <stdint.h>
#include <stdlib.h>

swl_gbm_buffer_t *swl_gbm_buffer_create(struct gbm_device *device, uint32_t width, uint32_t height, uint32_t format, uint32_t flags, uint32_t bpp) {
	swl_gbm_buffer_t *bo = calloc(1, sizeof(swl_gbm_buffer_t));

	bo->device = device;
	bo->render = gbm_device_get_fd(device);
	bo->bo = gbm_bo_create(device, width, height, format, flags);

	bo->pitch = gbm_bo_get_stride(bo->bo);
	bo->bpp = bpp;
	bo->width = width;
	bo->height = height;
	bo->size = bo->pitch * height;
	bo->handle = gbm_bo_get_handle(bo->bo).u32;

	return bo;
}

void swl_gbm_buffer_destroy(swl_gbm_buffer_t *bo) {
	gbm_bo_destroy(bo->bo);
	free(bo);
}
