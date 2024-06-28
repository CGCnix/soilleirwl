#include "soilleirwl/logger.h"
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

static void swl_data_source_offer(struct wl_client *client, struct wl_resource *source,
		const char *mime) {
	swl_info("%s\n", mime);
}

static void swl_data_source_set_actions(struct wl_client *client, struct wl_resource *source,
		uint32_t actions) {
}

static void swl_data_source_destroy(struct wl_client *client, struct wl_resource *source) {
}

static const struct wl_data_source_interface wl_data_source_impl = {
	.offer = swl_data_source_offer,
	.set_actions = swl_data_source_set_actions,
	.destroy = swl_data_source_destroy,
};

static void swl_data_device_release(struct wl_client *client, struct wl_resource *data_dev) {

}

static void swl_data_device_set_sel(struct wl_client *client, struct wl_resource *data_dev,
		struct wl_resource *source, uint32_t serial) {

}

static void swl_data_device_start_drag(struct wl_client *client, struct wl_resource *data_dev,
		struct wl_resource *source, struct wl_resource *origin, 
		struct wl_resource *icon, uint32_t serial) {

}

static const struct wl_data_device_interface wl_data_device_impl = {
	.release = swl_data_device_release,
	.set_selection = swl_data_device_set_sel,
	.start_drag = swl_data_device_start_drag,
};

static void swl_data_device_manager_create_data_src(struct wl_client *client,
		struct wl_resource *data_dev_man, uint32_t id) {
	struct wl_resource *source; 
	
	source = wl_resource_create(client, &wl_data_source_interface, 3, id);
	wl_resource_set_implementation(source, &wl_data_source_impl, NULL, NULL);

}

static void swl_data_device_manager_get_dev(struct wl_client *client,
		struct wl_resource *data_dev_man, uint32_t id, struct wl_resource *wl_seat) {
	struct wl_resource *device; 
	
	device = wl_resource_create(client, &wl_data_device_interface, 3, id);
	wl_resource_set_implementation(device, &wl_data_device_impl, NULL, NULL);


}

static const struct wl_data_device_manager_interface wl_data_device_manager_impl = {
	.create_data_source = swl_data_device_manager_create_data_src,
	.get_data_device = swl_data_device_manager_get_dev,
};

static void swl_data_device_man_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	struct wl_resource *data_dev_man;
	
	data_dev_man = wl_resource_create(client, &wl_data_device_manager_interface, version, id);
	wl_resource_set_implementation(data_dev_man, &wl_data_device_manager_impl, data, NULL);
}

void swl_create_data_dev_man(struct wl_display *display) {
	wl_global_create(display, &wl_data_device_manager_interface, 3, NULL, swl_data_device_man_bind);	
}
