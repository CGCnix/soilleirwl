CC=cc
CFLAGS=-O0 -g

WAYLAND_SCANNER=`pkg-config wayland-scanner --variable=wayland_scanner`
WAYLAND_DIR=`pkg-config wayland-protocols --variable=pkgdatadir`
