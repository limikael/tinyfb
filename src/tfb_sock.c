#include "tfb_sock.h"
#include "tfb_util.h"

void tfb_sock_handle_stream_event(tfb_stream_t *stream, int event) {
	tfb_sock_t *sock=stream->tag;

	if (sock->event_func)
		sock->event_func(sock,event);
}

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name, int proto) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->stream=tfb_stream_create(physical,name);
	sock->stream->tag=sock;
	sock->accepted=false;
	tfb_stream_event_func(sock->stream,tfb_sock_handle_stream_event);

	return sock;
}

tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, int id, char *name, int proto) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->stream=tfb_stream_create_controlled(link,name,id);
	sock->stream->tag=sock;
	sock->accepted=false;
	tfb_stream_event_func(sock->stream,tfb_sock_handle_stream_event);

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

char *tfb_sock_get_name(tfb_sock_t *sock) {
	return sock->stream->name;
}

bool tfb_sock_is_connected(tfb_sock_t *sock) {
	return tfb_stream_is_connected(sock->stream);
}

tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock) {
	return tfb_stream_get_deadline(sock->stream);
}

int tfb_sock_get_timeout(tfb_sock_t *sock) {
	return tfb_time_timeout(sock->stream->link->physical,tfb_sock_get_deadline(sock));
}

void tfb_sock_event_func(tfb_sock_t *sock, void (*event_func)(tfb_sock_t *sock, int event)) {
	sock->event_func=event_func;
}

int tfb_sock_send(tfb_sock_t *sock, uint8_t *data, size_t size) {
	return tfb_stream_send(sock->stream,data,size);
}

size_t tfb_sock_available(tfb_sock_t *sock) {
	return tfb_stream_available(sock->stream);
}

int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size) {
	return tfb_stream_recv(sock->stream,data,size);
}
