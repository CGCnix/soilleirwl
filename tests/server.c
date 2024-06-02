#include <soilleirwl/session.h>
#include <soilleirwl/input.h>
#include <soilleirwl/dev_man.h>
#include <soilleirwl/logger.h>

#include <wayland-server-core.h>
#include <wayland-server.h>


int swl_udev_backend_start(void *udev);	
int main(int argc, char **argv) {
	struct wl_display *display;

	swl_log_init(SWL_LOG_INFO, "/tmp/soilleir");

	display = wl_display_create();
	wl_display_add_socket_auto(display);

	swl_session_backend_t *session = swl_seatd_backend_create(display);
	swl_dev_man_backend_t *dev_man = swl_udev_backend_create(display);
	swl_input_backend_t *input = swl_libinput_backend_create(display, session, dev_man);
	
	swl_udev_backend_start(dev_man);

	wl_display_run(display);;

	return 0;
}
