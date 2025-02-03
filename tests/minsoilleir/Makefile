CC=clang
CFLAGS=-O0 -g -I /usr/include/libdrm/ -L ../../ -I ../../includes/
.SUFFIXES: .c .o
COBJS=server.o ipc.o

WAYLAND_SCANNER=`pkg-config wayland-scanner --variable=wayland_scanner`
WAYLAND_DIR=`pkg-config wayland-protocols --variable=pkgdatadir`

all: minsoilctrl screenshot minsoilleir


swl-screenshot-server.h: swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header swl-screenshot-unstable-v1.xml $@

swl-screenshot-client.h: swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) client-header swl-screenshot-unstable-v1.xml $@

swl-screenshot.c: swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code  swl-screenshot-unstable-v1.xml $@

xdg-shell-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-server.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

minsoilleir: $(COBJS)  swl-screenshot-server.h swl-screenshot.c xdg-shell-server.h xdg-shell-server.c
	$(CC) $(CFLAGS) -o $@ $(COBJS) swl-screenshot.c xdg-shell-server.c -lsoilleirwl -lwayland-server

minsoilctrl: minsoilctrl.c
	$(CC) $(CFLAGS) -o $@ minsoilctrl.c -lpng

screenshot: screenshot.c swl-screenshot.c swl-screenshot-client.h
	$(CC) $(CFLAGS) -o $@ screenshot.c swl-screenshot.c -lpng -lwayland-client

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
