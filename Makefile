.POSIX:
.SUFFIXES: .c .o
include config.mk

TARGET=libsoilleirwl.so
COBJS=src/egl.o src/backend/swl/input/libinput.o src/backend/swl/drm/libdrm.o src/backend/swl/hotplug/libudev.o src/backend/swl/sessions/libseat.o src/xdg-shell.o src/logger.o src/interfaces/swl_compositor.o src/interfaces/swl_data_dev_man.o src/backend/xcb/xcb.o src/backend/backend.o src/backend/swl/tty_backend.o src/allocator/gbm.o src/interfaces/swl_seat.o src/interfaces/swl_xdg_shell.o src/ext-image-capture-source-v1.o src/ext-foreign-toplevel-list-v1.o

CFILES=src/xdg-shell.c src/ext-image-capture-source-v1.c src/ext-foreign-toplevel-list-v1.c
CHEADS=includes/private/xdg-shell-server.h includes/private/xdg-shell-client.h  includes/private/ext-image-capture-source-v1-server.h includes/private/ext-foreign-toplevel-list-v1-server.h
CLIBSFLAGS=`pkg-config --cflags gbm xkbcommon wayland-server libseat libudev libinput libdrm glesv2 egl xcb xcb-dri3 xcb-present xcb-render xcb-renderutil`
CLIBS=`pkg-config --libs gbm xkbcommon wayland-server libseat libudev libinput libdrm egl glesv2 xcb xcb-dri3 xcb-present xcb-render xcb-renderutil`

all: $(CHEADS) $(CFILES) $(TARGET)

includes/private/ext-image-capture-source-v1-server.h:
	$(WAYLAND_SCANNER) server-header $(WAYLAND_DIR)/staging/ext-image-capture-source/ext-image-capture-source-v1.xml $@

src/ext-image-capture-source-v1.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/staging/ext-image-capture-source/ext-image-capture-source-v1.xml $@

includes/private/ext-foreign-toplevel-list-v1-server.h:
	$(WAYLAND_SCANNER) server-header $(WAYLAND_DIR)/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml $@

src/ext-foreign-toplevel-list-v1.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml $@

includes/private/xdg-shell-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

includes/private/xdg-shell-client.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

includes/private/wp_viewporter.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/viewporter/viewporter.xml $@

src/wp_viewporter.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/viewporter/viewporter.xml $@

includes/private/linux-dmabuf-server.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/linux-dmabuf/linux-dmabuf-v1.xml $@

src/zwp-linux-dmabuf.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/linux-dmabuf/linux-dmabuf-v1.xml $@

.c.o:
	$(CC) -c -fpic $(CLIBSFLAGS) $(CFLAGS) -o $@ $<

$(TARGET): $(INCLUDES) $(COBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COBJS) $(CLIBS) -shared

clean:
	rm -rf $(COBJS) $(TARGET)

clean_gen: clean
	rm -rf $(CHEADS) $(CFILES)
