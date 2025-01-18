.POSIX:
.SUFFIXES: .c .o
include config.mk

TARGET=soilleir
COBJS=src/egl.o src/backend/swl/input/libinput.o src/swl-screenshot-server.o src/backend/swl/drm/libdrm.o src/backend/swl/hotplug/libudev.o src/backend/swl/sessions/libseat.o src/xdg-shell-server.o tests/server.o src/logger.o src/interfaces/swl_compositor.o src/interfaces/swl_data_dev_man.o src/backend/xcb/xcb.o src/backend/backend.o src/backend/swl/tty_backend.o src/allocator/gbm.o
CLIBSFLAGS=`pkg-config --cflags gbm xkbcommon wayland-server libseat libudev libinput libdrm glesv2 egl xcb xcb-dri3 xcb-present`
CLIBS=`pkg-config --libs gbm xkbcommon wayland-server libseat libudev libinput libdrm egl glesv2 xcb xcb-dri3 xcb-present`

all: src/swl-screenshot-server.h src/xdg-shell-server.h $(TARGET) screenshot ipc-cli

ipc-cli: ./tests/ipc.c
	$(CC) ./tests/ipc.c `pkg-config --cflags libdrm libpng` -o $@ `pkg-config --libs libpng`

tests/swl-screenshot-client.h: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) client-header  ./protocols/swl-screenshot-unstable-v1.xml $@

tests/swl-screenshot-client.c: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code  ./protocols/swl-screenshot-unstable-v1.xml $@

screenshot: ./tests/screenshot.c tests/swl-screenshot-client.c tests/swl-screenshot-client.h
	$(CC) $(CFLAGS) -o $@ ./tests/screenshot.c ./tests/swl-screenshot-client.c -lwayland-client -lpng

src/swl-screenshot-server.h: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header  ./protocols/swl-screenshot-unstable-v1.xml $@

src/swl-screenshot-server.c: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code  ./protocols/swl-screenshot-unstable-v1.xml $@

src/xdg-shell-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell-server.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

.c.o:
	$(CC) -c $(CLIBSFLAGS) $(CFLAGS) -o $@ $<

$(TARGET): $(INCLUDES) $(COBJS) tests/server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COBJS) $(CLIBS)

clean:
	rm -rf bg-set screenshot src/xdg-shell.h src/xdg-shell.c $(COBJS) $(TARGET)
