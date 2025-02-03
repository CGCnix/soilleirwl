#include <stdbool.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libpng16/png.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/mman.h>


enum {
	SERVER_CHG_KEYBMAP,
	SERVER_SET_BACKGRN,
};

typedef struct{
	uint32_t opcode;
	uint32_t len;
}__attribute__((packed)) soilleir_ipc_msg_t;

typedef struct {
	uint32_t opcode;
	uint32_t len;
	int32_t height, width, stride, size;
	uint32_t depth;
	uint32_t format;
}__attribute__((packed)) soilleir_ipc_background_image;

typedef struct {
	uint32_t opcode;
	uint32_t len;
	uint16_t layout;
}__attribute__((packed)) soilleir_ipc_change_keymap;


void *readpng_get_image(FILE *infile, uint32_t *width, uint32_t *height,
		uint32_t *stride, int *depth, int *ctype) {
	uint8_t sig[8];
	static png_structp png_ptr;
	static png_infop info_ptr;

	fread(sig, 1, 8, infile);
	if(!png_check_sig(sig, 8)) {
		printf("This isn't a PNG file\n");
		return NULL;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr) {
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	png_init_io(png_ptr, infile);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, width, height, depth, ctype, NULL, NULL, NULL);
	printf("Color type: %d, %d, %d\n", *ctype, png_get_bit_depth(png_ptr, info_ptr), *depth);

	/* This should now work for most PNGs */
	if(*ctype == PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
	} else if(*ctype == PNG_COLOR_TYPE_GRAY && *depth < 8) {
		png_set_expand(png_ptr);
	}

	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}
	
	if(*depth == 16) {
		png_set_strip_16(png_ptr);
	}

	if(*ctype == PNG_COLOR_TYPE_GRAY || *ctype == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}

	png_bytepp row_pointers = calloc(sizeof(void*), *height);
	png_read_update_info(png_ptr, info_ptr);
	*stride = png_get_rowbytes(png_ptr, info_ptr);
	*ctype = png_get_color_type(png_ptr, info_ptr);
	*depth = png_get_bit_depth(png_ptr, info_ptr);
	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);
	printf("Color type: %d, %d, %d %d\n", *ctype, png_get_bit_depth(png_ptr, info_ptr), *depth, *stride);
	for(int y = 0; y < *height; y++) {
		row_pointers[y] = (png_byte*)malloc(*stride);
	}

	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, info_ptr);
	return row_pointers;
}

void shm_randomize(char *str) {
	unsigned long length = strlen(str);
	length -= 6;

	/*We don't care about random being **super random***/
	srand(time(NULL));

	for(; str[length]; ++length) {
		str[length] = (rand() % 26) + 'A';
	}
}

soilleir_ipc_background_image *create_buffer(uint32_t format, uint32_t width, uint32_t height, uint32_t stride, png_bytepp rows, int *out) {
	soilleir_ipc_background_image *buffer = calloc(1, sizeof(soilleir_ipc_background_image));
	struct wl_shm_pool *pool;
	char template[] = "/sterm-XXXXXX";
	int fd;
	fd = -1;

	/* loop until we have a valid fd or errno is
	* something other than EEXIST
	*/
	do {
		shm_randomize(template);
		fd = shm_open(template, O_EXCL | O_CREAT | O_RDWR, 0600);
	} while(fd < 0 && errno == EEXIST);

	if(fd < 0) {
		return NULL;
	}

	shm_unlink(template);
	buffer->len = sizeof(*buffer);
	buffer->opcode = SERVER_SET_BACKGRN;
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->size = buffer->stride * buffer->height;
	buffer->format = format;
	errno = 0;
	ftruncate(fd, buffer->size);

	uint8_t *data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	for(uint32_t y = 0; y < height; y++) {
		memcpy(&data[y * buffer->stride], rows[y], buffer->stride);
	}

	munmap(data, buffer->size);
	*out = fd;

	return buffer;
}

int send_ipc_msg(int32_t fd, int32_t fd_to_send, soilleir_ipc_msg_t *ipcmsg) {
	int client;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);

	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(int))];
	} control_un;
	//server_ipc_msg_t mesg;
	struct msghdr msg = { 0 };
	struct iovec iov[1] = { 0 };
	char buf[4096] = { 0 };

	struct cmsghdr *cmptr;
	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	iov[0].iov_len = ipcmsg->len;
	iov[0].iov_base = ipcmsg;
	msg.msg_iov = iov;
  msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	if(fd_to_send >= 0) {
		cmptr = CMSG_FIRSTHDR(&msg);
		cmptr->cmsg_len = CMSG_LEN(sizeof(int));
		cmptr->cmsg_level = SOL_SOCKET;
		cmptr->cmsg_type = SCM_RIGHTS;
		*((int *) CMSG_DATA(cmptr)) = fd_to_send;
	} else {
		msg.msg_controllen = 0;
		msg.msg_control = NULL;
	}

	if(sendmsg(fd, &msg, 0) <= 0) {
		printf("Error %d %d %m\n", fd, fd_to_send);
		return 0;
	}

	return 0;
}

#include <drm_fourcc.h>

void soilleir_ipc_set_bg(const char *swl_ipc, const char *image_name, bool invert) {
	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, swl_ipc);
	static soilleir_ipc_background_image *image;
	uint32_t width, height, stride;
	int depth, format;
	int sockfd, image_fd;
	FILE *fp;


	fp = fopen(image_name, "r");
	if(!fp) {
		printf("Failed to open image file: %s %s\n", image_name, strerror(errno));
	}

	png_bytepp rows = readpng_get_image(fp, 
			&width,
			&height,
			&stride,
			&depth,
			&format);
	if(!rows) {
		printf("Failed to read png file: %s\n", image_name);
		return;
	}

	switch(format) {
		case PNG_COLOR_TYPE_RGB:
			format = DRM_FORMAT_RGB888;
			break;
		case PNG_COLOR_TYPE_RGBA:
			format = DRM_FORMAT_RGBA8888;
			break;
		default:
			printf("Unsupported Color format: %d\n", format);
			close(image_fd);
			free(image);
			return; /*Bail*/
	}

	if(invert) {
		if(format == DRM_FORMAT_RGB888) {
			format = DRM_FORMAT_BGR888;
		} else if(format == DRM_FORMAT_RGBA8888) {
			format = DRM_FORMAT_BGRA8888;
		}
	}

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error failed to create socket: %s\n", strerror(errno));
		return;
	}

	if(connect(sockfd, (void*)&addr, sizeof(addr)) < 0) {
		printf("Error failed to connect to %s: %s\n", addr.sun_path, strerror(errno));
		return;
	}

	image = create_buffer(format, width, height, stride, rows, &image_fd);	
	for(uint32_t y = 0; y < height; ++y) {
		free(rows[y]);
	}
	free(rows);


	send_ipc_msg(sockfd, image_fd, (soilleir_ipc_msg_t*)image);	
	close(image_fd);
	free(image);
}

void soilleir_ipc_chg_km(const char *swl_ipc, const char *keymap) {
	struct sockaddr_un addr = { 0 };
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, swl_ipc);
	static soilleir_ipc_change_keymap keymap_msg;
	int sockfd;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error failed to create socket: %s\n", strerror(errno));
		return;
	}

	if(connect(sockfd, (void*)&addr, sizeof(addr)) < 0) {
		printf("Error failed to connect to %s: %s\n", addr.sun_path, strerror(errno));
		return;
	}

	if(strlen(keymap) != 2) {
		printf("Invalid keymap(%s) please input the two letter keycode for your map i.e. d.e. for germany\n", keymap);
		return;
	}
	
	keymap_msg.opcode = SERVER_CHG_KEYBMAP;
	keymap_msg.len = sizeof(keymap_msg);
	keymap_msg.layout = keymap[0] << 8 | keymap[1];
	send_ipc_msg(sockfd, -1, (soilleir_ipc_msg_t*)&keymap_msg);	
}


int main(int argc, char *argv[]) {
	static soilleir_ipc_background_image *image;
	uint32_t width, height, depth, format;
	int sockfd, image_fd;
	FILE *fp;
	const char *swl_ipc = getenv("SWL_IPC_SOCKET");
	const char *image_name = NULL;
	const char *keymap = NULL;
	bool invert;
	for(uint32_t i = 1; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] != '-') {
			if(argv[i][1] == 'n') {
				image_name = argv[i+1];
				i += 1;
			} else if(argv[i][1] == 'i') {
				invert = true;	
			} else if(argv[i][1] == 'k') {
				keymap = argv[i+1];
				i += 1;
			}
		} else {
			printf("Unknown Positional argument at %d value %s\n", i, argv[i]);
		}
	}

	if(image_name == NULL && keymap == NULL) {
		printf("Usage: %s -n <PATH_TO_PNG> -k <KEYMAPNAME>\n", argv[0]);
		return -1;
	}

	if(!swl_ipc) {
		printf("SWL_IPC_SOCKET env variable is not set\n");
		return-1;
	}
	

	if(image_name) {
		soilleir_ipc_set_bg(swl_ipc, image_name, invert);
	}

	if(keymap) {
		soilleir_ipc_chg_km(swl_ipc, keymap);	
	}
	
	return 0;
}
