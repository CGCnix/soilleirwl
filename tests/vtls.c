#include <sys/vt.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#define VTNAME "/dev/tty"
#define VT0NAME "/dev/tty1"
#define VTCON "/dev/console"

#define FD_VALID(fd) fd >= 0

bool test_is_console(int fd) {
	char kb_type = 0;

	return ioctl(fd, KDGKBTYPE, &kb_type) == 0;
}

int open_console(const char *path) {
	int fd;
	
	/*We just need an FD try RDONLY and WRONLY*/
	fd = open(path, O_RDONLY);
	if(fd < 0 && errno == EACCES) {
		fd = open(path, O_WRONLY);
	}

	if(fd < 0 || test_is_console(fd) == false) {
		if(FD_VALID(fd)) {
			close(fd);
		}
		return -1;
	}

	return fd;
}

int get_console_fd() {
	int fd;
	/* Complicated but which console
	 * the user has permission to open
	 * depends slightly on the system.
	 * so we test each VT_NAME above
	 * and then fall back to STDIN/OUT/ERR
	 */

	fd = open_console(VTNAME);
	if(FD_VALID(fd)) {
		return fd;
	}

	fd = open_console(VT0NAME);
	if(FD_VALID(fd)) {
		return fd;
	}

	fd = open_console(VTCON);
	if(FD_VALID(fd)) {
		return fd;
	}

	for(fd = 0; fd <= STDERR_FILENO; fd++) {
		if(test_is_console(fd)) {
			return fd;
		}
	}
	
	return -1;
}

const char *vt_mode_to_str(char mode) {
	switch(mode)  {
		case VT_AUTO: return "AUTO";
		case VT_PROCESS: return "PROCESS";
		case VT_ACKACQ: return "ACKACQ";
		default: return "Unknown";
	}
}

int main(int argc, char *argv[]) {
	int confd;
	struct vt_stat vtstat;
	struct vt_mode vtmode;

	confd = get_console_fd();
	errno = 0;
	printf("Confd: %d %m\n", confd);
	printf("PID: %d\n", getpid());
	if(ioctl(confd, VT_GETSTATE, &vtstat)) {
		printf("IOCTL FAILED %m\n");
	}
	
	if(ioctl(confd, VT_GETMODE, &vtmode)) {
		printf("IOCTL FAILED %m\n");
	}

	printf("Current VT MODE : %s\n", vt_mode_to_str(vtmode.mode));

	vtmode.mode = VT_AUTO;
	if(ioctl(confd, VT_SETMODE, &vtmode)) {
		printf("IOCTL FAILED %m\n");
	}
	
	while(true) {
		sleep(1);
	}

	printf("active console: %u\n", vtstat.v_active);

	return 0;
}
