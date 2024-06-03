include config.mk

TARGET=soilleir
COBJS=src/input/libinput.o src/drm/drm.o src/hotplug/udev.o src/sessions/seatd.o src/xdg-shell.o tests/server.o src/logger.o
CLIBSFLAGS=`pkg-config --cflags libdrm xkbcommon wayland-server libseat libudev libinput libdrm`
CLIBS=`pkg-config --libs libdrm xkbcommon wayland-server libseat libudev libinput libdrm`


all: src/xdg-shell.h $(TARGET)

src/xdg-shell.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

.c.o:
	$(CC) -c $(CLIBSFLAGS) $(CFLAGS) -o $@ $<

$(TARGET): $(COBJS) tests/server.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COBJS) $(CLIBS)

clean:
	rm -rf src/xdg-shell.h src/xdg-shell.c $(COBJS) $(TARGET)
