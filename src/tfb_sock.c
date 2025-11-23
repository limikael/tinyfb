#include "tfb_sock.h"
#include "tfb_util.h"

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name, int proto) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));
	sock->stream=tfb_stream_create(physical,name);
	return sock;
}

void tfb_sock_dispose(tfb_sock_t *sock) {
	tfb_stream_dispose(sock->stream);
	tfb_free(sock);
}

void tfb_sock_tick(tfb_sock_t *sock) {
	tfb_stream_tick(sock->stream);
}

int tfb_sock_get_id(tfb_sock_t *sock) {
	return sock->stream->id;
}
