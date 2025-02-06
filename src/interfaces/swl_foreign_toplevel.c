
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <soilleirwl/logger.h>
#include <soilleirwl/interfaces/swl_xdg_shell.h>
#include <soilleirwl/interfaces/swl_foreign_toplevel.h>

#include <private/ext-foreign-toplevel-list-v1-server.h>
#include <wayland-util.h>

struct _swl_foreign_toplevel_s {
	struct wl_global *global;

	struct wl_list list;
};

typedef struct {
	char identifier[32];
	swl_xdg_toplevel_t *toplevel;

	struct wl_list link;
} swl_foreign_toplevel_t;

#define SWL_FOREIGN_TOPLEVEL_VERSION 1

/*TODO: maybe consider making this indepenant as atm it's depenant on xdg_toplevels*/
void swl_foreign_toplevel_list_new_toplevel(swl_foreign_toplevel_list_t *list, swl_xdg_toplevel_t *toplevel) {
	
}

static void ext_foreign_toplevel_list_v1_stop(struct wl_client *client, struct wl_resource *list_resource) {

}

static void ext_foreign_toplevel_list_v1_destroy(struct wl_client *client, struct wl_resource *list_resource) {
	wl_resource_destroy(list_resource);
}

static void ext_foreign_toplevel_list_v1_resource_destroy(struct wl_resource *list_resource) {

}


struct ext_foreign_toplevel_list_v1_interface ext_foreign_toplevel_list_v1_impl = {
	.stop = ext_foreign_toplevel_list_v1_stop,
	.destroy = ext_foreign_toplevel_list_v1_destroy
};

static void swl_ext_foreign_toplevel_list_bind(struct wl_client *client, void *data,
    uint32_t version, uint32_t id) {
	swl_foreign_toplevel_list_t *list = data;
	struct wl_resource *resource;
	swl_foreign_toplevel_t *toplevel;

	resource = wl_resource_create(client, &ext_foreign_toplevel_list_v1_interface, 1, id);
	wl_resource_set_implementation(resource, &ext_foreign_toplevel_list_v1_impl, data, ext_foreign_toplevel_list_v1_resource_destroy);

	wl_list_for_each(toplevel, &list->list, link) {
		
	}
}

swl_foreign_toplevel_list_t *swl_foreign_toplevel_list_create(struct wl_display *display) {
	swl_foreign_toplevel_list_t *toplevel_list = calloc(1, sizeof(swl_foreign_toplevel_list_t));

	toplevel_list->global = wl_global_create(display, &ext_foreign_toplevel_list_v1_interface, SWL_FOREIGN_TOPLEVEL_VERSION, toplevel_list, swl_ext_foreign_toplevel_list_bind);
	wl_list_init(&toplevel_list->list);
	return toplevel_list;
}
