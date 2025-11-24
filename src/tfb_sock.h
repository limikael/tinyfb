#pragma once
#include "tfb_sock.h"
#include "tfb_time.h"
#include "tfb_link.h"

#define TFB_CONNECTION_TIMEOUT 5000
#define TFB_RESEND_BASE 10
#define TFB_RETRIES 5

#define TFB_EVENT_CONNECT 1
#define TFB_EVENT_CLOSE 2
#define TFB_EVENT_DATA 3

typedef struct tfb_sock tfb_sock_t;

struct tfb_sock {
	char *name;
	int id,resend_level,controlled_id;
	tfb_time_t activity_deadline,resend_deadline;
	tfb_link_t *link;
	uint8_t rx_buf[TFB_BUFSIZE], tx_buf[TFB_BUFSIZE];
	size_t rx_pending,rx_committed,tx_pending,tx_committed;
	void (*event_func)(tfb_sock_t *sock, int event);
	int session_id;
	void *tag;
	bool accepted;
};

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name);
tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, char *name, int id);
void tfb_sock_dispose(tfb_sock_t *sock);
void tfb_sock_tick(tfb_sock_t *sock);
int tfb_sock_send(tfb_sock_t *sock, uint8_t *data, size_t size);
size_t tfb_sock_available(tfb_sock_t *sock);
int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size);
tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock);
int tfb_sock_get_timeout(tfb_sock_t *sock);
void tfb_sock_event_func(tfb_sock_t *sock, void (*event_func)(tfb_sock_t *sock, int event));
bool tfb_sock_is_connected(tfb_sock_t *sock);
bool tfb_sock_is_controlled(tfb_sock_t *sock);
int tfb_sock_get_timeout(tfb_sock_t *sock);
