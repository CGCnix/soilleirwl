#include "./swl-screenshot-client.h"

#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <libpng16/png.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

typedef struct test_output {
	struct wl_output *output;
	const char *name;
	int32_t height, width;
	struct wl_buffer *buffer;
	void *data;
	struct wl_list link;
} test_output_t;

typedef struct test_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;	
	struct wl_list outputs;
	struct zswl_screenshot_manager *manager;
} test_client_t;

static void
randname(char *buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int
create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

static int
allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
        return -1;
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    /* Sent by the compositor when it's no longer using this buffer */
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};


static struct wl_buffer *get_frame(test_client_t *client, test_output_t *output) {
  int32_t width, height, stride, size;  
  
	width = output->width;
	height = output->height;
	
	/*TODO: don't assume format*/
	stride = width * 4;
  size = stride * height;

	int fd = allocate_shm_file(size);
	if (fd == -1) {
			return NULL;
	}

	uint32_t *data = mmap(NULL, size,
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
			close(fd);
			return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(client->shm, fd, size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
					width, height, stride, WL_SHM_FORMAT_XRGB8888);

	zswl_screenshot_manager_copy_output(client->manager, output->output, buffer, width, height, 0, 0);

	output->buffer = buffer;
	output->data = data;

	wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
	return buffer;
}


void wl_output_name(void *data, struct wl_output *output, const char *name) {
	printf("Output %p name: %s\n", output, name);
	test_output_t *test_output = data;
	test_output->name = strdup(name);
}

void wl_output_description(void *data, struct wl_output *output, const char *desc) {
}

void wl_output_scale(void *data, struct wl_output *output, int32_t scale) {
}

void wl_output_mode(void *data, struct wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
	test_output_t *test_output = data;
	test_output->width = width;
	test_output->height = height;
}

void wl_output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
		int32_t width, int32_t height, int32_t subpixel, const char *make,
		const char *model, int32_t transform) {
}

void wl_output_done(void *data, struct wl_output *output) {
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
	test_client_t *client = data;

	if(strcmp(wl_output_interface.name, interface) == 0) {
		test_output_t *output = calloc(1, sizeof(test_output_t));	
		output->output = wl_registry_bind(registry, name, &wl_output_interface, version);
		wl_output_add_listener(output->output, &output_listen, output);
		wl_list_insert(&client->outputs, &output->link);	
	} else if(strcmp(zswl_screenshot_manager_interface.name, interface) == 0) {
			client->manager = wl_registry_bind(registry, name, &zswl_screenshot_manager_interface, version);
	} else if(strcmp(wl_shm_interface.name, interface) == 0) {
		client->shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
	}

}

void wl_registry_global_rm(void *data, struct wl_registry *registry, uint32_t name) { 
	struct wl_output *output;

}

static struct wl_registry_listener reg_listen = {
	.global = wl_registry_global,
	.global_remove = wl_registry_global_rm,
};

int writeImage(char* filename, int width, int height, uint8_t *buffer, char *title)
{
   int code = 0;
   FILE *fp = NULL;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   png_bytep row = NULL;

		// Open file for writing (binary mode)
   fp = fopen(filename, "wb");
   if (fp == NULL) {
      fprintf(stderr, "Could not open file %s for writing\n", filename);
      code = 1;
      goto finalise;
   }

	    // Initialize write structure
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (png_ptr == NULL) {
      fprintf(stderr, "Could not allocate write struct\n");
      code = 1;
      goto finalise;
   }

   // Initialize info structure
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL) {
      fprintf(stderr, "Could not allocate info struct\n");
      code = 1;
      goto finalise;
   }

	    // Setup Exception handling
   if (setjmp(png_jmpbuf(png_ptr))) {
      fprintf(stderr, "Error during png creation\n");
      code = 1;
      goto finalise;
   }

	    png_init_io(png_ptr, fp);

   // Write header (8 bit colour depth)
   png_set_IHDR(png_ptr, info_ptr, width, height,
         8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

   // Set title
   if (title != NULL) {
      png_text title_text;
      title_text.compression = PNG_TEXT_COMPRESSION_NONE;
      title_text.key = "Title";
      title_text.text = title;
      png_set_text(png_ptr, info_ptr, &title_text, 1);
   }

   png_write_info(png_ptr, info_ptr);

	    // Allocate memory for one row (3 bytes per pixel - RGB)
   row = (png_bytep) malloc(3 * width * sizeof(png_byte));

   // Write image data
   int x, y;
   printf("%lu\n", sizeof(png_byte));
	 for (y=0 ; y<height ; y++) {
      for (x=0 ; x<width ; x++) {
         row[x*3 + 0] = (buffer)[y*(width*4) + (x * 4 + 2)]; // BGR to RGB conversion is handled here
         row[x*3 + 1] = (buffer)[y*(width*4) + (x * 4 + 1)];
         row[x*3 + 2] = (buffer)[y*(width*4) + (x * 4 + 0)];

			}
      png_write_row(png_ptr, row);
   }

   // End write
   png_write_end(png_ptr, NULL);

	    finalise:
   if (fp != NULL) fclose(fp);
   if (info_ptr != NULL) png_destroy_info_struct(png_ptr, &info_ptr);
   if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
   if (row != NULL) free(row);

   return code;
}


int main(int argc, char **argv) {
	char *filename = "swl_screenshot.png";
	bool dump = false;
	const char *monitor_name = NULL;
	test_client_t *client = calloc(1, sizeof(test_client_t));
	test_output_t *output;
	wl_list_init(&client->outputs);
	
	for(uint32_t i = 1; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] != '-') {
			if(argv[i][1] == 'n') {
				monitor_name = argv[i+1];
				i += 1;
			} else if(argv[i][1] == 'o') {
				filename = argv[i+1];
				i += 1;
			} else if(argv[i][1] == 'd') {
				dump = true;	
			}
		} else {
			printf("Unknown Positional argument at %d value %s\n", i, argv[i]);
		}
	}

	if(monitor_name == NULL && !dump) {
		printf("Please Supply the monitor to capture\n"
				"Value should be that of the wl_output_name event\n");
		return 1;
	} 
	if(filename == NULL && !dump) {
		printf("Error -o passed but no argument for filename given\n");
		return 1;
	}

	client->display = wl_display_connect(NULL);
	client->registry = wl_display_get_registry(client->display);
	wl_registry_add_listener(client->registry, &reg_listen, client);
	/*Complete two round trips one to bind registry outputs and screenshot
	 * manager the second to process output events;
	 */
	wl_display_roundtrip(client->display);
	
	if(!client->manager && !dump) {
		printf("Compositor support for swl_screenshot not found\n");
		return 1;
	}

	wl_display_roundtrip(client->display);

	if(dump) {
		return 1;
	} 
	
	printf("monitor_name %s\n", monitor_name);

	wl_list_for_each(output, &client->outputs, link) {
		printf("Output name: %s\n", output->name);
		if(strcmp(output->name, monitor_name) == 0) {
			get_frame(client, output);
			wl_display_roundtrip(client->display);
			writeImage(filename, output->width, output->height, output->data, "swl_screenshot");
		}
	}

	wl_display_roundtrip(client->display);
	
	return EXIT_SUCCESS;
}
