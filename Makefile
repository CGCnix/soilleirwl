include config.mk

TARGET=soilleir
COBJS=src/backend.o src/xdg-shell.o
CLIBSFLAGS=`pkg-config --cflags libdrm xkbcommon wayland-server`
CLIBS=`pkg-config --libs libdrm xkbcommon wayland-server`


all: src/xdg-shell.h $(TARGET)

src/xdg-shell.h:
	$(WAYLAND_SCANNER) server-header  $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

src/xdg-shell.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_DIR)/stable/xdg-shell/xdg-shell.xml $@

.c.o:
	$(CC) -c $(CLIBSFLAGS) $(CFLAGS) -o $@ $<

$(TARGET): $(COBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COBJS) $(CLIBS)

clean:
	rm -rf src/xdg-shell.h src/xdg-shell.c $(COBJS) $(TARGET)
