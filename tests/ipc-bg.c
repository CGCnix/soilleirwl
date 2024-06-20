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

typedef struct {
	uint32_t opcode;
	uint32_t len;
	int32_t height, width, stride, size;
	uint32_t depth;
	uint32_t format;
}__attribute__((packed)) swl_ipc_background_image;

void *readpng_get_image(FILE *infile, uint32_t *width, uint32_t *height,
		uint32_t *depth, uint32_t *ctype) {
	uint8_t sig[8];
	static png_structp png_ptr;
	static png_infop info_ptr;


	fread(sig, 1, 8, infile);
	if(!png_check_sig(sig, 8)) {
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
	
	/*Just bail
	 * todo: fix and decode other PNGS
	 */
	if(*ctype != PNG_COLOR_TYPE_RGBA) {
		printf("Invalide color type\n");
		return NULL;
	}

	
	png_bytepp row_pointers = calloc(sizeof(void*), *height);
	for(int y = 0; y < *height; y++) {
    row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));
  }

	png_read_image(png_ptr, row_pointers);
	printf("Color type: %d, %d, %d\n", *ctype, PNG_COLOR_TYPE_RGBA, *depth);
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

swl_ipc_background_image *create_buffer(uint32_t format, uint32_t width, uint32_t height, png_bytepp rows, int *out) {
	swl_ipc_background_image *buffer = calloc(1, sizeof(swl_ipc_background_image));
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

	buffer->width = width;
	buffer->height = height;
	buffer->stride = buffer->width * 4;
	buffer->size = buffer->stride * buffer->height;
	buffer->format = format;
	errno = 0;
	ftruncate(fd, buffer->size);

	uint8_t *data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	for(uint32_t y = 0; y < height; y++) {
		memcpy(&data[y * buffer->stride], rows[y], buffer->stride);
	}

	*out = fd;

	return buffer;
}

int send_fd(int32_t fd, int32_t fd_to_send, swl_ipc_background_image *image) {
	int client;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);

	union {
    struct cmsghdr    cm;
    char              control[CMSG_SPACE(sizeof(int))];
  } control_un;
	//server_ipc_msg_t mesg;
  struct msghdr msg = { 0 };
  struct iovec iov[1] = { 0 };
	char buf[4096] = { 0 };



	struct cmsghdr *cmptr;
	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	iov[0].iov_len = sizeof(*image);
	iov[0].iov_base = image;
	msg.msg_iov = iov;
  msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	cmptr = CMSG_FIRSTHDR(&msg);
  cmptr->cmsg_len = CMSG_LEN(sizeof(int));
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = fd_to_send;
	
	if(sendmsg(fd, &msg, 0) <= 0) {
		printf("Error %d %d %m\n", fd, fd_to_send);
		return 0;
	}


	return 0;

}

#include <drm_fourcc.h>

int main(int argc, char *argv[]) {
	struct sockaddr_un addr = {0};
	static swl_ipc_background_image *image;
	uint32_t width, height, depth, format;
	int sockfd, image_fd;
	FILE *fp;
	const char *swl_ipc = getenv("SWL_IPC_SOCKET");
	const char *image_name = NULL;
	bool invert;
	for(uint32_t i = 1; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] != '-') {
			if(argv[i][1] == 'n') {
				image_name = argv[i+1];
				i += 1;
			} else if(argv[i][1] == 'i') {
				invert = true;	
			}
		} else {
			printf("Unknown Positional argument at %d value %s\n", i, argv[i]);
		}
	}

	if(image_name == NULL) {
		printf("Usage: %s -n <PATH_TO_PNG>\n", argv[0]);
		return -1;
	}

	if(!swl_ipc) {
		printf("SWL_IPC_SOCKET env variable is not set\n");
		return-1;
	}
	
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, swl_ipc);
	
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sockfd < 0) {
		printf("Error failed to create socket: %s\n", strerror(errno));
		return -1;
	}

	if(connect(sockfd, (void*)&addr, sizeof(addr)) < 0) {
		printf("Error failed to connect to %s: %s\n", addr.sun_path, strerror(errno));
		return -1;
	}

	fp = fopen(image_name, "r");
	
	png_bytepp rows = readpng_get_image(fp, 
			&width, 
			&height, 
			&depth, 
			&format);
	
	image = create_buffer(format, width, height, rows, &image_fd);	
	/*Convert PNG format to something Server understands*/
	if(invert) {
		image->format = DRM_FORMAT_RGBA8888;
	} else {
		image->format = DRM_FORMAT_BGRA8888;
	}

	send_fd(sockfd, image_fd, image);	
	
	return 0;
}
