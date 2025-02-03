#pragma once

#include <wayland-server.h>


typedef struct {
	int fd;
	int lock;
	struct wl_event_source *source;
} server_ipc_sock;

#include "./minsoilleir.h"


int soilleir_ipc_deinit(soilleir_server_t *soilleir);
int soilleir_ipc_init(soilleir_server_t *soilleir);
