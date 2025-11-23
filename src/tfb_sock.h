#pragma once
#include "tfb_stream.h"
#include "tfb_util.h"

typedef struct tfb_sock tfb_sock_t;

struct tfb_sock {
	tfb_stream_t *stream;
	int proto;
};

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name, int proto);
void tfb_sock_dispose(tfb_sock_t *sock);
void tfb_sock_tick(tfb_sock_t *sock);
int tfb_sock_get_id(tfb_sock_t *sock);
