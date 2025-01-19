#include <soilleirwl/xcursor.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* Quick and dirty test of Xcursor files */
static inline size_t xcursor_get_header_size(FILE *fp) {
	uint32_t size;
	uint32_t ntoc;
	long origin;

	origin = ftell(fp);
	fseek(fp, SWL_XCURSOR_HEADER_SIZE_OFF, SEEK_SET);
	fread(&size, sizeof(uint32_t), 1, fp);
	fseek(fp, SWL_XCURSOR_HEADER_NTOC_OFF, SEEK_SET);
	fread(&ntoc, sizeof(uint32_t), 1, fp);
	fseek(fp, origin, SEEK_SET);

	return size + ntoc * sizeof(swl_xcur_toc_t);
}

swl_xcur_header_t *read_xcursor_header(FILE *fp) {
	swl_xcur_header_t *header;
	size_t size;

	size = xcursor_get_header_size(fp);

	header = calloc(1, size);
	if(header) {
		fread(header, 1, size, fp);
	}
	return header;
}

swl_xcur_image_t *find_xcursor_image(swl_xcur_header_t *header, uint32_t preffered, FILE *fp) {
	swl_xcur_image_t *image = NULL;
	size_t size = 0;
	uint32_t height;
	uint32_t width;
	if(!header || !fp) return NULL;

	for(uint32_t i = 0; i < header->ntoc; ++i) {
		printf("%d\n", header->toc[i].subtype);
		if(header->toc[i].type == SWL_XCURSOR_TYPE_IMAGE && preffered == header->toc[i].subtype) {
			fseek(fp, header->toc[i].position, SEEK_SET);
			fread(&size, sizeof(uint32_t), 1, fp);
			fseek(fp, header->toc[i].position + 16, SEEK_SET);
			fread(&width, sizeof(uint32_t), 1, fp);
			fread(&height, sizeof(uint32_t), 1, fp);
			fseek(fp, header->toc[i].position, SEEK_SET);
			size += width * sizeof(uint32_t) * height;
			image = calloc(1, size);
			fread(image, 1, size, fp);
			break;	
		}
	}

	return image;
}

int main(int argc, char **argv) {
	Display *dpy;
	Screen *screen;
	Window win;
	Window root;
	XEvent ev;
	XImage *image;
	GC gc;

	if(argc < 2) {
		printf("Usage: %s CURSOR_FILE\n", argv[0]);
		return 0;
	}

	FILE *fp = fopen(argv[1], "rb");

	swl_xcur_header_t *header = read_xcursor_header(fp);
	swl_xcur_image_t *cursor = find_xcursor_image(header, 24, fp);
	if(!cursor) {
		printf("Unable to find cursor size of 24\n");
		return -1;
	}

	dpy = XOpenDisplay(NULL);
	screen = XScreenOfDisplay(dpy, 0);
	root = XRootWindowOfScreen(screen);
	win = XCreateSimpleWindow(dpy, root, 0, 0, 640, 480, 1, 0xffffffff, 0xffffff);
	XSelectInput(dpy, win, ExposureMask);
	XMapWindow(dpy, win);
	image = XCreateImage(dpy, screen->root_visual, 24, ZPixmap, 0, (void*)cursor->pixels, cursor->height, 
			cursor->height, 32, cursor->width * 4);
	gc = XDefaultGCOfScreen(screen);
	XFlush(dpy);
	
	while(1) {
		XNextEvent(dpy, &ev);
		XPutImage(dpy, win, gc, image, 0, 0, 0, 0, cursor->width, cursor->height);
		XFlush(dpy);
	}

	return EXIT_SUCCESS;
}
