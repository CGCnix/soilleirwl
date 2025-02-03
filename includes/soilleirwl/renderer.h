#pragma once
#include "soilleirwl/allocator/gbm.h"
typedef struct swl_renderer swl_renderer_t;
typedef struct swl_renderer_target swl_renderer_target_t;
typedef struct swl_texture swl_texture_t;

#include <soilleirwl/interfaces/swl_output.h>
#include <stdint.h>


/* Begin function for binding buffers and any 
 * other setup that the render API requires
 * Need some way to bind an output which would
 * probably involve importing DMABUF of the monitors
 * buffers.
 *
 * Ughhh this is headach okay so we probably want to 
 * make it so the server can create an swl_texture_t
 * this texture they can create from client buffers and other 
 * buffers they want to render.
 *
 * The renderer will create the texture to ensure
 * it is a compatible texture e.g. an EGL or VK image
 * But we also want the renderer to be able take in a swl_output
 * buffer and turn it into a render surface/target
 *
 * So in effect both the server and output would need access to 
 * the renderer to allow both of them to render using one context. Which would
 * work much the same as it does now where the server will render to the 
 * SWL_OUTPUT by calling the function pointer stored in the output.
 * Trouble with that is either Server needs access to renderer to turn 
 * it's surface buffers into compatible render textures or the drmbackend's 
 * output needs to manager the surfaces spefically the creation and destruction 
 * of Render textures which is messy as it would mean an output needs to be informed
 * when a surface is destroyed otherwise it may hold onto old data.
 *
 * or we can give the server access to renderer and give the renderer 
 * acess to the private swl_drm_output_t. Then the server could pass the 
 * output that the frame should be presented on i.e.
 * DRMBackend -> (sends output frame event to compositor) Compositor -> 
 * (Sends output to renderer along with data it wants rendered) -> renderer 
 * (Renders all of the desiered data) -> render returns to compositor -> 
 * Compositor returns to DRM backend -> drm backend flips buffers rendered data on screen
 */ 

typedef void (*SWL_RENDER_BEGIN)(swl_renderer_t *renderer);
typedef void (*SWL_RENDER_END)(swl_renderer_t *renderer);
typedef void (*SWL_RENDER_CLEAR)(swl_renderer_t *renderer, float r, float g, float b, float a);
typedef void (*SWL_RENDER_ATTACH_OUTPUT)(swl_renderer_t *renderer, swl_output_t *output);
typedef void (*SWL_RENDER_ATTACH_TARGET)(swl_renderer_t *renderer, swl_renderer_target_t *target);
typedef void (*SWL_RENDER_DESTROY_TARGET)(swl_renderer_t *renderer, swl_renderer_target_t *target);
typedef swl_texture_t *(*SWL_RENDER_CREATE_TEXTURE)(swl_renderer_t *renderer, uint32_t width, uint32_t height, uint32_t format, void *data);
typedef swl_renderer_target_t *(*SWL_RENDER_CREATE_TARGET)(swl_renderer_t *renderer, swl_gbm_buffer_t *buffer);
typedef void (*SWL_RENDER_DESTROY_TEXTURE)(swl_renderer_t *renderer, swl_texture_t *texture);
typedef void (*SWL_RENDER_TEXTURE_DRAW)(swl_renderer_t *render, swl_texture_t *texture_in, int32_t x, int32_t y, int32_t sx, int32_t sy);
typedef void (*SWL_RENDER_DESTROY)(swl_renderer_t *renderer);
typedef void (*SWL_RENDERER_COPY_FROM)(swl_renderer_t *renderer, void *dst, uint32_t height, uint32_t width, uint32_t x, uint32_t y, uint32_t format);

struct swl_renderer {
	SWL_RENDER_BEGIN begin;
	SWL_RENDERER_COPY_FROM copy_from;
	SWL_RENDER_END end;
	SWL_RENDER_CLEAR clear;
	SWL_RENDER_ATTACH_OUTPUT attach_output;
	SWL_RENDER_ATTACH_TARGET attach_target;
	SWL_RENDER_CREATE_TARGET create_target;
	SWL_RENDER_DESTROY_TARGET destroy_target;
	SWL_RENDER_CREATE_TEXTURE create_texture;
	SWL_RENDER_DESTROY_TEXTURE destroy_texture;
	SWL_RENDER_TEXTURE_DRAW draw_texture;
	SWL_RENDER_DESTROY destroy;
};


swl_renderer_t *swl_egl_renderer_create_by_fd(int drm_fd);	
