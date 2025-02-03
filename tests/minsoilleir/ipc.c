#include "./ipc.h"
#include "./minsoilleir.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <sys/un.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <soilleirwl/logger.h>
#include <soilleirwl/interfaces/swl_output.h>

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

static int soilleir_ipc_set_bgimage(struct msghdr *msg, soilleir_server_t *soilleir) {
	soilleir_ipc_background_image *image = msg->msg_iov[0].iov_base;
	int recvfd = -1;
  struct cmsghdr *cmptr;

	if ((cmptr = CMSG_FIRSTHDR(msg)) != NULL &&
    cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
    if (cmptr->cmsg_level != SOL_SOCKET && cmptr->cmsg_type != SCM_RIGHTS) {
			swl_error("IPC Client sent a cmsg without SOL_SOCKET or SCM_RIGHTS\n");
			return 0;
		}
		recvfd = *((int *) CMSG_DATA(cmptr));
	}
	if(recvfd < 0) {
		swl_error("IPC recv fd for bg image invalid\n");
		return 0;
	}

	soilleir->bg = mmap(0, image->size, PROT_READ | PROT_WRITE, MAP_SHARED, recvfd, 0);
	close(recvfd);
	soilleir_output_t *output;
	wl_list_for_each(output, &soilleir->outputs, link) {
		output->common->background = output->common->renderer->create_texture(output->common->renderer, image->width, image->height, image->format, soilleir->bg);
	}

	munmap(soilleir->bg, image->size);
	return 0;	
}

int soilleir_ipc_chg_keymap(struct msghdr *msg, soilleir_server_t *soilleir) {
	soilleir_ipc_change_keymap *keymap = msg->msg_iov->iov_base;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	struct xkb_rule_names names;
	char layout[3] = { 0 };

	layout[0] = keymap->layout >> 8;
	layout[1] = keymap->layout & 0xff;
	swl_seat_set_keymap(soilleir->seat, layout);

	return 0;
}

int server_ipc(int32_t fd, uint32_t mask, void *data) {
	int client, recvfd;
	soilleir_server_t *soilleir = data;
	soilleir_ipc_msg_t *ipcmsg;
	struct sockaddr addr;
	socklen_t len = sizeof(addr);
	client = accept(fd, &addr, &len);

	union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_un;
  struct msghdr msg = { 0 };
  struct iovec iov[1] = { 0 };
	char buf[4096] = { 0 };
	
	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	iov[0].iov_len = 4095;
	iov[0].iov_base = buf;
	msg.msg_iov = iov;
  msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	
	if(recvmsg(client, &msg, 0) < 0) {
		swl_error("Erorr %d %m\n", client);
		return 0;
	}
	/*HACK WE KINDA JUST ASSUME WE GOT THE WHOLE MESSAGE*/
	close(client);
	ipcmsg = (void*)buf;
	
	switch (ipcmsg->opcode) {
		case SERVER_SET_BACKGRN:
			return soilleir_ipc_set_bgimage(&msg, soilleir);
			break;
		case SERVER_CHG_KEYBMAP:
			return soilleir_ipc_chg_keymap(&msg, soilleir);
			break;
	}

	return 0;
}

#define SOILLEIR_IPC_PATH "%s/soil-%d"
#define SOILLEIR_IPC_LOCK "%s/soil-%d.lock"

int soilleir_ipc_deinit(soilleir_server_t *soilleir) {
	char lock_addr[256] = { 0 };
	struct sockaddr_un addr = { 0 };
	pid_t pid = getpid();
	const char *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");

	snprintf(addr.sun_path, sizeof(addr.sun_path), SOILLEIR_IPC_PATH, xdg_rt_dir, pid);
	snprintf(lock_addr, sizeof(lock_addr), SOILLEIR_IPC_LOCK, xdg_rt_dir, pid);
	
	wl_event_source_remove(soilleir->ipc.source);


	unlink(addr.sun_path);
	close(soilleir->ipc.fd);	
	flock(soilleir->ipc.lock, LOCK_UN | LOCK_NB);
	close(soilleir->ipc.lock);
	remove(lock_addr);
	
	return 0;
}

int soilleir_ipc_init(soilleir_server_t *soilleir) {
	char lock_addr[256] = { 0 };
	struct sockaddr_un addr = { 0 };
	struct stat stat;
	pid_t pid = getpid();
	const char *xdg_rt_dir = getenv("XDG_RUNTIME_DIR");
	struct wl_event_loop *loop;

	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), SOILLEIR_IPC_PATH, xdg_rt_dir, pid);
	snprintf(lock_addr, sizeof(lock_addr), SOILLEIR_IPC_LOCK, xdg_rt_dir, pid);
	soilleir->ipc.lock = open(lock_addr, O_CREAT | O_CLOEXEC | O_RDWR,
														(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
	if(soilleir->ipc.lock < 0) {
		swl_error("Unabled to open lock file %s: %s\n", lock_addr, strerror(errno));
		return -1;
	}

	/*If we can lock this file we can then unlink the IPC socket should it exist*/
	if(flock(soilleir->ipc.lock, LOCK_EX | LOCK_NB) < 0) {
		/* Due to fact we use the pid in the lock file this shouldn't be 
		 * possible ever as if this process is killed the lock will be released
		 * by the OS. It should only happen if we call this function twice
		 */
		swl_error("Failed to lock file %s: %s\n", lock_addr, strerror(errno));
		return -1;
	}

	if(lstat(addr.sun_path, &stat) < 0 && errno != ENOENT) {
		/*If we failed to stat the file for some reason other than 
		 * this socket not existing
		 */
		swl_error("Failed to state file %s: %s\n", addr.sun_path, strerror(errno));
		return -1;
	}	else {
		/*Success so stale old socket exists*/
		unlink(addr.sun_path);
	}

	soilleir->ipc.fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if(bind(soilleir->ipc.fd, (void*)&addr, sizeof(addr)) == -1) {
		swl_error("Failed bind to socket %s: %s\n", addr.sun_path, strerror(errno));
		return -1;
	}
	loop = wl_display_get_event_loop(soilleir->display);
	
	soilleir->ipc.source = wl_event_loop_add_fd(loop, soilleir->ipc.fd, WL_EVENT_READABLE, server_ipc, soilleir);
	
	listen(soilleir->ipc.fd, 128);
	setenv("SWL_IPC_SOCKET", addr.sun_path, 1);	
	return 0;
}


