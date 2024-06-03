#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


void wl_output_name(void *data, struct wl_output *output, const char *name) {
	printf("Output %p name: %s\n", output, name);
}

void wl_output_description(void *data, struct wl_output *output, const char *desc) {
	printf("Output %p desc: %s\n", output, desc);
}

void wl_output_scale(void *data, struct wl_output *output, int32_t scale) {
	printf("Output %p scale: %d\n", output, scale);
}

void wl_output_mode(void *data, struct wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
	printf("Output %p mode: %dx%d@%dHz\n", output, width, height, refresh);
}

void wl_output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
		int32_t width, int32_t height, int32_t subpixel, const char *make,
		const char *model, int32_t transform) {
	printf("Output %p geom:\n"
			"X&Y: %d %d\n"
			"width & height: %dmm %dmm\n"
			"subpixel: %d\n"
			"make: %s\n"
			"model: %s\n"
			"transform: %d\n",
			output, x, y, width, height, subpixel, make, model, transform);

}

void wl_output_done(void *data, struct wl_output *output) {
	wl_output_release(output);
}


struct wl_output_listener output_listen = {
	.mode = wl_output_mode,
	.name = wl_output_name,
	.done = wl_output_done,
	.scale = wl_output_scale,
	.geometry = wl_output_geometry,
	.description = wl_output_description,
};

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version) { 
	struct wl_output *output;
	
	if(strcmp(wl_output_interface.name, interface) == 0) {
		output = wl_registry_bind(registry, name, &wl_output_interface, version);
		wl_output_add_listener(output, &output_listen, NULL);
	}
}

void wl_registry_global_rm(void *data, struct wl_registry *registry, uint32_t name) { 
	struct wl_output *output;

}

static struct wl_registry_listener reg_listen = {
	.global = wl_registry_global,
	.global_remove = wl_registry_global_rm,
};

int main(void) {
	struct wl_display *display = wl_display_connect(NULL);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &reg_listen, NULL);

	while(wl_display_dispatch(display)) {

	}

	return EXIT_SUCCESS;
}
