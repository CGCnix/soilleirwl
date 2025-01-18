#include <soilleirwl/backend/input.h>
#include <soilleirwl/backend/hotplug.h>
#include <soilleirwl/backend/session.h>
#include <soilleirwl/logger.h>
#include <soilleirwl/interfaces/swl_input_device.h>

#include <wayland-server-core.h>
#include <libinput.h>

#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-util.h>

typedef struct swl_libinput_device {
	int fd, dev_id;
	struct wl_list link;
} swl_libinput_device_t;

typedef struct swl_libinput_backend {
	swl_input_backend_t common;

	struct libinput *ctx;

	struct wl_display *display;
	struct wl_event_source *readable;

	struct wl_list devices;

	struct wl_listener device_added;
	struct wl_listener deactivate;
	struct wl_listener activate;

	/**/
	swl_session_backend_t *session;
	swl_hotplug_backend_t *hotplug;
} swl_libinput_backend_t;

void swl_libinput_device_added_listener(struct wl_listener *listener, void *data) {
	swl_libinput_backend_t *libinput = wl_container_of(listener, libinput, device_added);

	libinput_path_add_device(libinput->ctx, data);
}

void swl_libinput_deactivate(struct wl_listener *listener, void *data) {
	swl_libinput_backend_t *libinput = wl_container_of(listener, libinput, deactivate);
	
	libinput_suspend(libinput->ctx);
}

void swl_libinput_activate(struct wl_listener *listener, void *data) {
	swl_libinput_backend_t *libinput = wl_container_of(listener, libinput, activate);
	
	libinput_resume(libinput->ctx);
}

swl_libinput_device_t *swl_libinput_get_device_by_fd(struct wl_list *devices, int fd) {
	swl_libinput_device_t *device;
	wl_list_for_each(device, devices, link) {
		if(device->fd == fd) return device;
	}

	return NULL;
}

int swl_libinput_open_res(const char *path, int flags, void *data) {
	swl_libinput_device_t *device;
	swl_libinput_backend_t *libinput = data;

	device = calloc(1, sizeof(swl_libinput_device_t));

	device->dev_id = libinput->session->open_dev(libinput->session, path, &device->fd);
	wl_list_insert(&libinput->devices, &device->link);
	
	/*This can happen if the device no longer exists*/
	if(device->dev_id < 0) {
		free(device);
		wl_display_terminate(libinput->display);
		return -1;
	}

	return device->fd;
}

void swl_libinput_close_res(int fd, void *data) {
	swl_libinput_backend_t *libinput = data;
	swl_libinput_device_t *device = swl_libinput_get_device_by_fd(&libinput->devices, fd);

	libinput->session->close_dev(libinput->session, device->dev_id);
	close(device->fd);
	wl_list_remove(&device->link);
	free(device);
}

static const struct libinput_interface swl_libinput_interface = {
	.open_restricted = swl_libinput_open_res,
	.close_restricted = swl_libinput_close_res,
};

int swl_libinput_readable(int fd, uint32_t mask, void *data) {
	swl_libinput_backend_t *libinput = data;
	struct libinput_event *event;
	struct libinput_event_keyboard *keyboard;
	struct libinput_event_pointer *pointer;
	struct libinput_device *device;
	swl_input_dev_t *input;
		
	libinput_dispatch(libinput->ctx);

	while((event = libinput_get_event(libinput->ctx))) {
		enum libinput_event_type type = libinput_event_get_type(event);
		device = libinput_event_get_device(event);

		switch(type) {
			case LIBINPUT_EVENT_DEVICE_ADDED: {
				swl_input_dev_t *swl_dev = calloc(1, sizeof(swl_input_dev_t));
				wl_signal_init(&swl_dev->button);
				wl_signal_init(&swl_dev->key);
				wl_signal_init(&swl_dev->motion);
				/*Inform Compositor
				 *It can store this device if it wishes
				 *Or just bind listeners to it
				 */
				wl_signal_emit(&libinput->common.new_input, swl_dev);
				libinput_device_set_user_data(device, swl_dev);
				break;
			}
			case LIBINPUT_EVENT_KEYBOARD_KEY: {
				keyboard = libinput_event_get_keyboard_event(event);
				input = libinput_device_get_user_data(device);
				swl_key_event_t key = { 0 };

				key.key = libinput_event_keyboard_get_key(keyboard);
				key.time = libinput_event_keyboard_get_time(keyboard);
				key.state = libinput_event_keyboard_get_key_state(keyboard);
				key.device = input;
				wl_signal_emit(&input->key, &key);
				break;
			}
			case LIBINPUT_EVENT_POINTER_MOTION: {
				swl_motion_event_t motion = { 0 };
				pointer = libinput_event_get_pointer_event(event);
				input = libinput_device_get_user_data(device);
				motion.dx = libinput_event_pointer_get_dx(pointer);
				motion.dy = libinput_event_pointer_get_dy(pointer);
				wl_signal_emit(&input->motion, &motion);
				break;
			}
			case LIBINPUT_EVENT_POINTER_BUTTON: {

			}
			case LIBINPUT_EVENT_DEVICE_REMOVED: {
				/*TODO;*/
				break;
			}
			default: {
				swl_warn("Unhanled Event: %d\n", type);
				break;
			}
		}
	}

	return 0;
}

int swl_libinput_backend_stop(swl_input_backend_t *input) {
	return 0;
}

int swl_libinput_backend_start(swl_input_backend_t *input) {
	return 0;
}

void swl_libinput_backend_destroy(swl_input_backend_t *input) {
	swl_libinput_backend_t *libinput;

	libinput = (swl_libinput_backend_t*)input;
	
	wl_event_source_remove(libinput->readable);
	libinput_suspend(libinput->ctx);
	libinput_unref(libinput->ctx);
	free(libinput);
}

swl_input_backend_t *swl_libinput_backend_create(struct wl_display *display,
		swl_session_backend_t *session, swl_hotplug_backend_t *hotplug) {
	swl_libinput_backend_t *libinput;
	struct wl_event_loop *loop;
	
	loop = wl_display_get_event_loop(display);

	if(!display && !loop) {
		return NULL;
	}
	
	libinput = calloc(1, sizeof(swl_libinput_backend_t));
	libinput->ctx = libinput_path_create_context(&swl_libinput_interface, libinput);
	libinput->readable = wl_event_loop_add_fd(loop, libinput_get_fd(libinput->ctx), WL_EVENT_READABLE, swl_libinput_readable, libinput);	

	libinput->session = session;
	libinput->hotplug = hotplug;
	libinput->display = display;

	wl_signal_init(&libinput->common.new_input);
	wl_list_init(&libinput->devices);

	libinput->common.SWL_INPUT_BACKEND_STOP = swl_libinput_backend_stop;
	libinput->common.SWL_INPUT_BACKEND_START = swl_libinput_backend_start;
	libinput->common.SWL_INPUT_BACKEND_DESTROY = swl_libinput_backend_destroy;

	libinput->activate.notify = swl_libinput_activate;
	libinput->deactivate.notify = swl_libinput_deactivate;
	libinput->device_added.notify = swl_libinput_device_added_listener;
	
	wl_signal_add(&session->activate, &libinput->activate);
	wl_signal_add(&session->disable, &libinput->deactivate);
	wl_signal_add(&hotplug->new_input, &libinput->device_added);
	
	return (swl_input_backend_t*)libinput;
}
