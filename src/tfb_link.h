#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "tfb_physical.h"
#include "tfb_time.h"
#include "tfb_frame.h"

#define TFB_CA_DELAY_MIN 2
#define TFB_CA_DELAY_MAX 5
#define TFB_LINK_SEND_URGENT 1
#define TFB_LINK_SEND_OWNED 2

typedef struct tfb_link tfb_link_t;

typedef enum {
	TFB_LINK_RX_INIT,
	TFB_LINK_RX_RECEIVE,
	TFB_LINK_RX_ESCAPE,
	TFB_LINK_RX_COMPLETE,
} tfb_link_rx_state_t;

struct tfb_link {
	tfb_physical_t *physical;
	tfb_frame_t *rx_frame;
	tfb_frame_t *tx_frame;
	tfb_link_rx_state_t rx_state;
	tfb_time_t bus_available_deadline;
	bool tx_frame_owned;
	uint32_t num_sent;
};

tfb_link_t *tfb_link_create(tfb_physical_t *tfb_physical);
void tfb_link_dispose(tfb_link_t *link);
bool tfb_link_send(tfb_link_t *link, tfb_frame_t *frame, int flags);
void tfb_link_unsend(tfb_link_t *link, tfb_frame_t *frame);
void tfb_link_notify_bus_activity(tfb_link_t *link);
void tfb_link_tick(tfb_link_t *link);
bool tfb_link_is_bus_available(tfb_link_t *link);
tfb_frame_t *tfb_link_peek(tfb_link_t *link);
void tfb_link_consume(tfb_link_t *link);
tfb_time_t tfb_link_get_deadline(tfb_link_t *link);
uint32_t tfb_link_get_num_sent(tfb_link_t *link);
uint32_t tfb_link_get_num_transmitted(tfb_link_t *link);
bool tfb_link_is_transmitted(tfb_link_t *link, uint32_t sendnum);
