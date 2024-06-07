#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#include <stdio.h>
#include <stdlib.h>

void wl_registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version) { 
	printf("%s\n", interface);
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
