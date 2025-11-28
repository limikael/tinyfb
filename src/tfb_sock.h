#pragma once
#include "tfb_sock.h"
#include "tfb_time.h"
#include "tfb_link.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TFB_CONNECTION_TIMEOUT 5000

#define TFB_RESEND_BASE 100
#define TFB_RETRIES 8

#define TFB_EVENT_CONNECT 1
#define TFB_EVENT_CLOSE 2
#define TFB_EVENT_DATA 3

#define TFB_SOCK_QUEUE_SIZE 4

typedef struct tfb_sock tfb_sock_t;

struct tfb_sock {
	char *name;
	int id,controlled_id;
	int resend_level,resend_sendnum;
	tfb_time_t activity_deadline,resend_deadline;
	tfb_link_t *link;
	void (*event_func)(tfb_sock_t *sock, int event);
	int session_id;
	void *tag;
	bool accepted;
	tfb_frame_t *tx_queue[TFB_SOCK_QUEUE_SIZE];
	tfb_frame_t *data;
	size_t tx_queue_len,num_sent,num_received;
};

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name);
tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, char *name, int id);
void tfb_sock_dispose(tfb_sock_t *sock);
void tfb_sock_tick(tfb_sock_t *sock);
int tfb_sock_send(tfb_sock_t *sock, uint8_t *data, size_t size);
size_t tfb_sock_available(tfb_sock_t *sock);
bool tfb_sock_is_send_available(tfb_sock_t *sock);
int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size);
tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock);
int tfb_sock_get_timeout(tfb_sock_t *sock);
void tfb_sock_event_func(tfb_sock_t *sock, void (*event_func)(tfb_sock_t *sock, int event));
bool tfb_sock_is_connected(tfb_sock_t *sock);
bool tfb_sock_is_controlled(tfb_sock_t *sock);
int tfb_sock_get_timeout(tfb_sock_t *sock);
bool tfb_sock_is_flushed(tfb_sock_t *sock);

#ifdef __cplusplus
}
#endif
