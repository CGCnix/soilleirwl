#include "soilleirwl/display.h"
#include <soilleirwl/session.h>
#include <soilleirwl/input.h>
#include <soilleirwl/dev_man.h>
#include <soilleirwl/logger.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>

typedef struct {

} swl_surface_t;

typedef struct {

	struct wl_list link;
}	swl_client_t;

typedef struct {
	struct wl_display *display;

	swl_session_backend_t *session;
	swl_dev_man_backend_t *dev_man;
	swl_input_backend_t *input;
	swl_display_backend_t *backend;

	struct wl_list clients;
} soilleir_server_t;

int swl_udev_backend_start(void *udev);	
int main(int argc, char **argv) {
	soilleir_server_t soilleir;

	swl_log_init(SWL_LOG_INFO, "/tmp/soilleir");

	soilleir.display = wl_display_create();
	wl_display_add_socket_auto(soilleir.display);

	soilleir.session = swl_seatd_backend_create(soilleir.display);
	soilleir.dev_man = swl_udev_backend_create(soilleir.display);
	soilleir.input = swl_libinput_backend_create(soilleir.display, soilleir.session, soilleir.dev_man);
	soilleir.backend = swl_drm_create_backend(soilleir.display, soilleir.session);

	swl_udev_backend_start(soilleir.dev_man);
	swl_drm_backend_start(soilleir.backend);

	wl_display_run(soilleir.display);
	swl_drm_backend_stop(soilleir.backend);

	return 0;
}
