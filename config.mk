CC=clang
CFLAGS=-O0 -g -I includes -fsanitize=address

WAYLAND_SCANNER=`pkg-config wayland-scanner --variable=wayland_scanner`
WAYLAND_DIR=`pkg-config wayland-protocols --variable=pkgdatadir`
