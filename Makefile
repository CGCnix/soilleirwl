.POSIX:
.SUFFIXES: .c .o
include config.mk

TARGET=soilleir
COBJS=src/egl.o src/backend/swl/input/libinput.o src/swl-screenshot-server.o src/backend/swl/drm/libdrm.o src/backend/swl/hotplug/libudev.o src/backend/swl/sessions/libseat.o src/xdg-shell-server.o tests/server.o src/logger.o src/interfaces/swl_compositor.o src/interfaces/swl_data_dev_man.o src/backend/xcb/xcb.o src/backend/backend.o src/backend/swl/tty_backend.o src/allocator/gbm.o src/interfaces/swl_seat.o src/wl_viewporter.o
CLIBSFLAGS=`pkg-config --cflags gbm xkbcommon wayland-server libseat libudev libinput libdrm glesv2 egl xcb xcb-dri3 xcb-present`
CLIBS=`pkg-config --libs gbm xkbcommon wayland-server libseat libudev libinput libdrm egl glesv2 xcb xcb-dri3 xcb-present`

all: includes/private/swl-screenshot-server.h includes/private/xdg-shell-server.h includes/private/wl_viewporter.h src/wl_viewporter.c includes/private/linux-dmabuf-server.h src/linux-dmabuf-server.c src/swl-screenshot-server.c src/xdg-shell-server.c $(TARGET) screenshot ipc-cli

ipc-cli: ./tests/ipc.c
	$(CC) ./tests/ipc.c `pkg-config --cflags libdrm libpng` -o $@ `pkg-config --libs libpng`

tests/swl-screenshot-client.h: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) client-header  ./protocols/swl-screenshot-unstable-v1.xml $@

tests/swl-screenshot-client.c: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code  ./protocols/swl-screenshot-unstable-v1.xml $@

screenshot: ./tests/screenshot.c tests/swl-screenshot-client.c tests/swl-screenshot-client.h
	$(CC) $(CFLAGS) -o $@ ./tests/screenshot.c ./tests/swl-screenshot-client.c -lwayland-client -lpng

includes/private/swl-screenshot-server.h: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header  ./protocols/swl-screenshot-unstable-v1.xml $@

src/swl-screenshot-server.c: ./protocols/swl-screenshot-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code  ./protocols/swl-screenshot-unstable-v1.xml $@

includes/private/xdg-shell-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell-server.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

includes/private/wl_viewporter.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/viewporter/viewporter.xml $@

src/wl_viewporter.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/viewporter/viewporter.xml $@


includes/private/linux-dmabuf-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/linux-dmabuf/linux-dmabuf-v1.xml $@

src/linux-dmabuf-server.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/linux-dmabuf/linux-dmabuf-v1.xml $@


.c.o:
	$(CC) -c $(CLIBSFLAGS) $(CFLAGS) -o $@ $<

$(TARGET): $(INCLUDES) $(COBJS) tests/server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COBJS) $(CLIBS)

clean:
	rm -rf include/private/swl-screenshot-server.h include/private/xdg-shell-server.h include/private/linux-dmabuf-server.h src/linux-dmabuf-server.c src/swl-screenshot-server.c src/xdg-shell-server.c ipc-cli screenshot $(COBJS) $(TARGET)
