#pragma once
#include "tfb_stream.h"
#include "tfb_util.h"
#include "tfb_time.h"

typedef struct tfb_sock tfb_sock_t;

struct tfb_sock {
	tfb_stream_t *stream;
	int proto;
	void (*event_func)(tfb_sock_t *sock, int event);
};

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name, int proto);
tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, int id, char *name, int proto);
void tfb_sock_dispose(tfb_sock_t *sock);
void tfb_sock_tick(tfb_sock_t *sock);
int tfb_sock_get_id(tfb_sock_t *sock);
char *tfb_sock_get_name(tfb_sock_t *sock);
bool tfb_sock_is_connected(tfb_sock_t *sock);
tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock);
int tfb_sock_get_timeout(tfb_sock_t *sock);
void tfb_sock_event_func(tfb_sock_t *sock, void (*event_func)(tfb_sock_t *sock, int event));
int tfb_sock_send(tfb_sock_t *sock, uint8_t *data, size_t size);
int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size);

size_t tfb_sock_available(tfb_sock_t *sock);
