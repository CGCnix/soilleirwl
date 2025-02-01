#include <soilleirwl/backend/hotplug.h>
#include <soilleirwl/logger.h>

#include <wayland-server.h>
#include <libudev.h>

#include <stdlib.h>

typedef struct {
	swl_hotplug_backend_t common;

	struct udev *udev;

	struct udev_monitor *input_monitor;
	struct udev_monitor *drm_monitor;

	struct wl_event_source *readable;
} swl_libudev_backend_t;

/*TODO: I am unsure how I am going to handle the drm
 * devices because I want to export their backends to 
 * be in the main function.
 *
 * My choices 
 *
 * 1) are to make a signal/callback function the main 
 * function can provide to have it get notified about drm cards
 * that exist then it can handle their creation passing a device
 * structure with alerts for events. That the newly created backend
 * can use to be alerted when certain hotplug events happen.
 *
 * 2) Create them here in udev code and then export them. But that 
 * means having to recreate them in every hotplug/device management backend
 *
 * 3) Create like a global event manager for DRM where it recieves an add event
 * and it adds a DRM backend to some linked list and it will just recieve the path
 * and the event type string from udev and then it will convert that into which 
 * drm card it should apply that event to.
 *
 * And then other than that it's about deciding which data structure to add 
 * the cards backends to as currently I am thinking a linked list would be best
 * the only downside is O(n) access timing. But that shouldn't be to major
 * as it's unlikely a user will have enough drm cards to severally impact the
 * timings.
 */

int swl_libudev_backend_start(swl_hotplug_backend_t *hotplug) {
	swl_libudev_backend_t *udev = (swl_libudev_backend_t*)hotplug;
	struct udev_enumerate *inputs;
	const char *sysname;
	const char *devpath;
	const char *subsystem;
	struct udev_device *device;
	struct udev_list_entry *entry, *head;

	inputs = udev_enumerate_new(udev->udev);
	udev_enumerate_add_match_subsystem(inputs, "input");
	
	udev_enumerate_add_match_sysname(inputs, "event[0-9]*");

	udev_enumerate_scan_devices(inputs);
	
	head = udev_enumerate_get_list_entry(inputs);
	udev_list_entry_foreach(entry, head) {
		device = udev_device_new_from_syspath(udev->udev, udev_list_entry_get_name(entry));
		if(udev_device_get_devnode(device)) {
			wl_signal_emit(&udev->common.new_input, (void*)udev_device_get_devnode(device));
		}
		udev_device_unref(device);
	}

	udev_enumerate_unref(inputs);
	return 0;
}

int swl_libudev_backend_stop(swl_hotplug_backend_t *hotplug) {
	return 0;
}

void swl_libudev_backend_destroy(swl_hotplug_backend_t *hotplug) {
	swl_libudev_backend_t *udev;

	udev = (swl_libudev_backend_t*)hotplug;
	
	udev_unref(udev->udev);
	free(udev);
}

swl_hotplug_backend_t *swl_libudev_backend_create(struct wl_display *display) {
	swl_libudev_backend_t *udev = calloc(1, sizeof(swl_libudev_backend_t));

	wl_signal_init(&udev->common.new_input);
	udev->udev = udev_new();
	udev->common.start = swl_libudev_backend_start;
	udev->common.stop = swl_libudev_backend_stop;
	udev->common.destroy = swl_libudev_backend_destroy;

	return (swl_hotplug_backend_t*)udev;
}
