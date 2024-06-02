#include <soilleirwl/dev_man.h>
#include <soilleirwl/logger.h>

#include <wayland-server.h>
#include <libudev.h>

#include <stdlib.h>

typedef struct {
	swl_dev_man_backend_t common;

	struct udev *udev;

	struct udev_monitor *input_monitor;
	struct udev_monitor *drm_monitor;

	struct wl_event_source *readable;
} swl_udev_backend_t;


int swl_udev_backend_start(void *data) {
	swl_udev_backend_t *udev = data;
	struct udev_enumerate *inputs;
	const char *sysname;
	const char *devpath;
	const char *subsystem;
	struct udev_device *device;
	struct udev_list_entry *entry, *head;

	inputs = udev_enumerate_new(udev->udev);
	udev_enumerate_add_match_subsystem(inputs, "input");
	udev_enumerate_add_match_subsystem(inputs, "drm");
	
	udev_enumerate_add_match_sysname(inputs, "event[0-9]*");
	udev_enumerate_add_match_sysname(inputs, "card[0-9]*");

	udev_enumerate_scan_devices(inputs);
	
	swl_info("Starting udev\n");
	head = udev_enumerate_get_list_entry(inputs);
	udev_list_entry_foreach(entry, head) {
		device = udev_device_new_from_syspath(udev->udev, udev_list_entry_get_name(entry));
		if(udev_device_get_devnode(device)) {
			swl_info("Sysname: %s\nDevnode %s\n", udev_list_entry_get_name(entry), udev_device_get_devnode(device));
		}
		//wl_signal_emit(&udev->common.new_input, udev_device_get_devnode(device));

		udev_device_unref(device);
	}
	exit(1);
	return 0;
}


swl_dev_man_backend_t *swl_udev_backend_create(struct wl_display *display) {
	swl_udev_backend_t *udev = calloc(1, sizeof(swl_udev_backend_t));

	wl_signal_init(&udev->common.new_input);
	udev->udev = udev_new();


	return (swl_dev_man_backend_t*)udev;
}
