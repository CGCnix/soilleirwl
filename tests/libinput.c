#include <libinput.h>

#include <libudev.h>

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

/* This is just me testing how libinput devices work
 * spefically how the user data set in a device works
 * as I want to use device data to transmit some data
 * 
 * Feel free to use this code for whatever you what
 */

int open_res(const char *path, int flags, void *data) {
	printf("%s\n", path);
	return open(path, flags);
}

void close_res(int fd, void *data) {
	close(fd);
}

const struct libinput_interface libinput_interface = {
	.open_restricted = open_res,
	.close_restricted = close_res,
};

int main(int argc, char *argv[]) {
	struct udev *udev;
	struct libinput *li;
	struct libinput_event *event;
	struct libinput_device *device;
	struct pollfd pfd;

	udev = udev_new();
	li = libinput_udev_create_context(&libinput_interface, NULL, udev);
	libinput_udev_assign_seat(li, "seat0");

	pfd.fd =  libinput_get_fd(li);
	pfd.events = POLLIN;
 
	while (poll(&pfd, 1, -1)) {
		libinput_dispatch(li);
		while((event = libinput_get_event(li))) {
		uint32_t type = libinput_event_get_type(event);
		
		printf("Event Type: %d\n", type);
		device = libinput_event_get_device(event);
		printf("Device: %s %p\n", libinput_device_get_name(device), device);
		if(type == LIBINPUT_EVENT_DEVICE_ADDED) {
			libinput_device_set_user_data(device, "Test data\n");
		} else {
			printf("%s\n", (char *)libinput_device_get_user_data(device));
		}

		// handle the event here
		libinput_event_destroy(event);
		}
	}

	return EXIT_SUCCESS;
}
