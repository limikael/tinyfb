#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "tfb_physical.h"
#include "tfb_time.h"

#define TFB_BUFSIZE 64
#define TFB_CA_DELAY_MIN 2
#define TFB_CA_DELAY_MAX 5
#define TFB_LINK_QUEUE_LEN 8
#define TFB_LINK_URGENT 1

typedef struct tfb_link tfb_link_t;

typedef enum {
	TFB_LINK_RX_INIT,
	TFB_LINK_RX_RECEIVE,
	TFB_LINK_RX_ESCAPE,
	TFB_LINK_RX_COMPLETE,
} tfb_link_rx_state_t;

struct tfb_link {
	tfb_physical_t *physical;
	uint8_t rx_buf[TFB_BUFSIZE];
	uint32_t rx_size;
	tfb_link_rx_state_t rx_state;
	tfb_time_t bus_available_deadline;
	uint8_t tx_buf[TFB_BUFSIZE];
	uint32_t tx_size;
};

tfb_link_t *tfb_link_create(tfb_physical_t *tfb_physical);
bool tfb_link_send(tfb_link_t *link, uint8_t *data, size_t size, int flags);
uint8_t *tfb_link_peek(tfb_link_t *link);
size_t tfb_link_peek_size(tfb_link_t *link);
void tfb_link_consume(tfb_link_t *link);
void tfb_link_dispose(tfb_link_t *link);
void tfb_link_tick(tfb_link_t *link);
void tfb_link_notify_bus_activity(tfb_link_t *link);
bool tfb_link_is_bus_available(tfb_link_t *link);
tfb_time_t tfb_link_get_deadline(tfb_link_t *link);
