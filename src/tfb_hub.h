#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tfb_link.h"
#include "tfb_time.h"
#include "tfb_sock.h"

#define TFB_ENOTCONN 1
#define TFB_ENOSPC 2
#define TFB_EOPNOTSUPP 3

#define TFB_ANNOUNCEMENT_INTERVAL 1000

typedef struct tfb_hub tfb_hub_t;

struct tfb_hub {
	tfb_link_t *link;
	int session_id;
	tfb_time_t announcement_deadline;
	tfb_sock_t *socks[16];
	size_t num_socks;
	void (*connect_func)(tfb_hub_t *hub, tfb_sock_t *sock);
	int proto;
};

tfb_hub_t *tfb_hub_create(tfb_physical_t *physical);
void tfb_hub_dispose(tfb_hub_t *hub);
void tfb_hub_tick(tfb_hub_t *hub);
tfb_time_t tfb_hub_get_deadline(tfb_hub_t *hub);
int tfb_hub_get_timeout(tfb_hub_t *hub);
void tfb_hub_connect_func(tfb_hub_t *hub, void (*connect_func)(tfb_hub_t *hub, tfb_sock_t *sock));
