#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <wayland-util.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <soilleirwl/renderer.h>
#include <soilleirwl/interfaces/swl_surface.h>
#include <soilleirwl/interfaces/swl_compositor.h>

#include <private/xdg-shell-server.h>


#define SWL_COMPOSITOR_VERSION 6
#define SWL_REGION_VERSION 1
#define SWL_SURFACE_VERSION 6
#define SWL_SUBCOMPOSITOR_VERSION 1
#define SWL_SUBSURFACE_VERSION 1
#define SWL_CALLBACK_VERSION 1

static void swl_surface_handle_frame(struct wl_client *client, struct wl_resource *surface_res,
		uint32_t id) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);

	surface->frame = wl_resource_create(client, &wl_callback_interface,  SWL_CALLBACK_VERSION, id);
	wl_resource_set_implementation(surface->frame, NULL, NULL, NULL);	
}

static void swl_surface_handle_set_buffer_scale(struct wl_client *client,
		struct wl_resource *surface_res, int32_t scale) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);

	surface->pending.scale = scale;
	surface->pending_changes |= SWL_SURFACE_PENDING_SCALE;
}

static void swl_surface_handle_set_buffer_transform(struct wl_client *client,
		struct wl_resource *surface_res, int32_t transform) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);

	surface->pending.transform = transform;
	surface->pending_changes |= SWL_SURFACE_PENDING_TRANSFORM;
}

static void swl_surface_handle_attach(struct wl_client *client, struct wl_resource *surface_res, struct wl_resource *buffer, int32_t x, int32_t y) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);
	surface->pending.buffer.buffer = buffer;
	surface->pending_changes |= SWL_SURFACE_PENDING_BUFFER;

	/*Old style offsets*/
	if(wl_resource_get_version(surface_res) < WL_SURFACE_OFFSET_SINCE_VERSION) {
		surface->pending.buffer.x = x;
		surface->pending.buffer.y = y;
	}
}

static void swl_surface_handle_damage(struct wl_client *client, struct wl_resource *surface_res, 
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

static void swl_surface_handle_damage_buffer(struct wl_client *client, 
		struct wl_resource *surface_res, int32_t x, int32_t y, int32_t width, int32_t height) {

}

static void swl_surface_handle_offset(struct wl_client *client, struct wl_resource *surface_res, 
		int32_t x, int32_t y) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);
	
	if(wl_resource_get_version(surface_res) >= WL_SURFACE_OFFSET_SINCE_VERSION) {
		surface->pending.buffer.x = x;
		surface->pending.buffer.y = y;
	}
}

static void swl_surface_handle_set_input_region(struct wl_client *client, struct wl_resource *surface_res, struct wl_resource *region) {

}

static void swl_surface_handle_set_opaque_region(struct wl_client *client, struct wl_resource *surface_res, struct wl_resource *region) {

}


static void swl_surface_handle_commit(struct wl_client *client, struct wl_resource *surface_res) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);
	swl_subsurface_t *subsurface;
	struct wl_shm_buffer *buffer;
	uint32_t width, height, format;	
	void *data;

	if(surface->role == NULL) {
		wl_client_post_implementation_error(client, "Commit on unconfigured surface\n");
		return;
	}
	surface->role->precommit(client, surface, surface->role_resource);

	wl_list_for_each(subsurface, &surface->subsurfaces, link) {
		/*Commit Synced Surfaces*/
		if(subsurface->sync) {
			swl_surface_handle_commit(client, subsurface->surface->resource);
		}
	}

	if(surface->pending_changes & SWL_SURFACE_PENDING_BUFFER) {
		if(surface->buffer.buffer && surface->buffer.buffer != surface->pending.buffer.buffer) {
			wl_buffer_send_release(surface->buffer.buffer);
		}
		surface->buffer.buffer = surface->pending.buffer.buffer;
		
		surface->buffer.x = surface->pending.buffer.x;
		surface->buffer.y = surface->pending.buffer.y;
	}

	/*Clear Pendinging*/
	memset(&surface->pending, 0, sizeof(swl_surface_state_t));
	surface->pending_changes = 0;
	surface->role->postcommit(client, surface, surface->role_resource);
}

static void swl_surface_handle_destroy(struct wl_client *client, struct wl_resource *surface_res) {
	wl_resource_destroy(surface_res);
}

static void swl_surface_resource_destroy(struct wl_resource *surface_res) {
	swl_surface_t *surface = wl_resource_get_user_data(surface_res);
	
	if(!surface) {
		return;
	}
	
	free(surface);
}

static const struct wl_surface_interface swl_surface_implementation = {
	.frame = swl_surface_handle_frame,
	.set_buffer_scale = swl_surface_handle_set_buffer_scale,
	.set_buffer_transform = swl_surface_handle_set_buffer_transform,
	.attach = swl_surface_handle_attach,
	.damage = swl_surface_handle_damage,
	.damage_buffer = swl_surface_handle_damage_buffer,
	.offset = swl_surface_handle_offset,
	.set_input_region = swl_surface_handle_set_input_region,
	.set_opaque_region = swl_surface_handle_set_opaque_region,
	.commit = swl_surface_handle_commit,
	.destroy = swl_surface_handle_destroy,
};

static void swl_compositor_create_surface(struct wl_client *client, struct wl_resource *compositor_res, uint32_t id) {
	swl_surface_t *surface;
	swl_compositor_t *compositor = wl_resource_get_user_data(compositor_res);

	surface = calloc(1, sizeof(swl_surface_t));
	if(!surface) {
		wl_client_post_no_memory(client);
		return;
	}
	
	/*Init Subsurfaces list*/
	wl_list_init(&surface->subsurfaces);

	/*Initialize some basic info*/
	surface->scale = 1;
	surface->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	surface->input_region = NULL;
	surface->opaque_region = NULL;
	surface->pending_changes = SWL_SURFACE_PENDING_NONE;	

	surface->resource = wl_resource_create(client, &wl_surface_interface, SWL_SURFACE_VERSION, id);
	if(!surface->resource) {
		free(surface); /*free the surface*/
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(surface->resource, &swl_surface_implementation, surface, swl_surface_resource_destroy);
	wl_signal_emit(&compositor->new_surface, surface);
}

/*TODO Implement*/
static void wl_region_destroy(struct wl_client *client, struct wl_resource *region) {

}

static void wl_region_add(struct wl_client *client, struct wl_resource *region,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

static void wl_region_subtract(struct wl_client *client, struct wl_resource *region,
		int32_t x, int32_t y, int32_t width, int32_t height) {

}

static const struct wl_region_interface region_implementation = {
	.destroy = wl_region_destroy,
	.add = wl_region_add,
	.subtract = wl_region_subtract,
};

static void swl_compositor_create_region(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *region = wl_resource_create(client, &wl_region_interface, 1, id);
	wl_resource_set_implementation(region, &region_implementation, NULL, NULL);
	/*TODO*/
}

static const struct wl_compositor_interface swl_compositor_implementation = {
	.create_region = swl_compositor_create_region,
	.create_surface = swl_compositor_create_surface,
};

static void swl_compositor_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *compositor_resource;
	swl_compositor_t *compositor = data;

	compositor_resource = wl_resource_create(client, &wl_compositor_interface, version, id);
	wl_resource_set_implementation(compositor_resource, &swl_compositor_implementation, compositor, NULL);
}

void swl_compositor_destroy(swl_compositor_t *compositor) {
	wl_global_destroy(compositor->wl_compositor);
	free(compositor);
}

swl_compositor_t *swl_compositor_create(struct wl_display *display, void *data) {
	swl_compositor_t *compositor = calloc(1, sizeof(swl_compositor_t));
	
	wl_signal_init(&compositor->new_surface);
	wl_signal_init(&compositor->new_region);

	compositor->data = data;
	compositor->wl_compositor = wl_global_create(display, &wl_compositor_interface, SWL_COMPOSITOR_VERSION, compositor, swl_compositor_bind);

	return compositor;
}


static void swl_subsurface_set_sync(struct wl_client *client, struct wl_resource *subsurface_resource) {
	swl_subsurface_t *subsurface = wl_resource_get_user_data(subsurface_resource);
	subsurface->sync = true;
}

static void swl_subsurface_set_desync(struct wl_client *client, struct wl_resource *subsurface_resource) {
	swl_subsurface_t *subsurface = wl_resource_get_user_data(subsurface_resource);
	subsurface->sync = false;
}

static void swl_subsurface_destroy(struct wl_client *client, struct wl_resource *subsurface) {
	wl_resource_destroy(subsurface);
}

static void swl_subsurface_place_above(struct wl_client *client, struct wl_resource *subsurface,
		struct wl_resource *sibling) {

}

static void swl_subsurface_place_below(struct wl_client *client, struct wl_resource *subsurface,
		struct wl_resource *sibling) {

}

static void swl_subsurface_set_pos(struct wl_client *client, struct wl_resource *resource,
		int32_t x, int32_t y) {
	swl_subsurface_t *subsurface = wl_resource_get_user_data(resource);

	subsurface->position.x = x;
	subsurface->position.y = y;
}

static void swl_subsurface_resource_destroy(struct wl_resource *subsurface_resource) {
	swl_subsurface_t *subsurface = wl_resource_get_user_data(subsurface_resource);
	/* TODO I don't think based on what I've read roles should be reassignable
	 * We need to be careful if this client doesn't destroy this surface correctly
	 */
	wl_list_remove(&subsurface->link);
	free(subsurface);
}

static const struct wl_subsurface_interface swl_subsurface_impl = {
	.destroy = swl_subsurface_destroy,
	.set_sync = swl_subsurface_set_sync,
	.set_desync = swl_subsurface_set_desync,
	.place_above = swl_subsurface_place_above,
	.place_below = swl_subsurface_place_below,
	.set_position = swl_subsurface_set_pos,
};

static void swl_subsurface_precommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {
		
}

static void swl_subsurface_postcommit(struct wl_client *client, swl_surface_t *surface, struct wl_resource *resource) {

}

swl_surface_role_t swl_subsurface_role =  {
	.precommit = swl_subsurface_precommit,
	.postcommit = swl_subsurface_postcommit,
};

static void swl_subcompositor_handle_destroy(struct wl_client *client, struct wl_resource *subcompositor) {
	wl_resource_destroy(subcompositor);
}

static void swl_subcompositor_resource_destroy(struct wl_resource *subcompositor) {

}


static void swl_subcompositor_get_subsurface(struct wl_client *client,
		struct wl_resource *subcompositor, uint32_t id, 
		struct wl_resource *surface, struct wl_resource *parent) {
	swl_subsurface_t *subsurface = calloc(1, sizeof(swl_subsurface_t));
	subsurface->sync = true;
	subsurface->surface = wl_resource_get_user_data(surface);
	subsurface->parent = wl_resource_get_user_data(parent);
	wl_list_insert(&subsurface->parent->subsurfaces, &subsurface->link);
	subsurface->resource = wl_resource_create(client, &wl_subsurface_interface, SWL_SUBSURFACE_VERSION, id);
	wl_resource_set_implementation(subsurface->resource, &swl_subsurface_impl, subsurface, swl_subsurface_resource_destroy);
	
	subsurface->surface->role = &swl_subsurface_role;
	subsurface->surface->role_resource = subsurface->resource;
}


static const struct wl_subcompositor_interface swl_subcompositor_implementation = {
	.destroy = swl_subcompositor_handle_destroy,
	.get_subsurface = swl_subcompositor_get_subsurface,
};

static void swl_subcompositor_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *subcompositor;
	
	subcompositor = wl_resource_create(client, &wl_subcompositor_interface, version, id);
	wl_resource_set_implementation(subcompositor, &swl_subcompositor_implementation, data, swl_subcompositor_resource_destroy);
}

void swl_subcompositor_destroy(swl_subcompositor_t *subcompositor) {
	wl_global_destroy(subcompositor->wl_subcompositor);
	free(subcompositor);
}

swl_subcompositor_t *swl_subcompositor_create(struct wl_display *display) {
	swl_subcompositor_t *subcompositor = calloc(1, sizeof(swl_subcompositor_t));
	subcompositor->wl_subcompositor = wl_global_create(display, 
			&wl_subcompositor_interface, SWL_SUBCOMPOSITOR_VERSION, 
			display, swl_subcompositor_bind);

	wl_signal_init(&subcompositor->wl_subsurface);
	return subcompositor;
}
