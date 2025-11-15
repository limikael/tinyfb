#pragma once
#include "tfb_internal.h"

typedef struct tfb_link tfb_link_t;

tfb_link_t *tfb_link_create();
void tfb_link_dispose(tfb_link_t *link);
void tfb_link_frame_func(tfb_link_t *link, void (*func)(tfb_link_t *link, uint8_t *data, size_t size));
void tfb_link_rx_push_byte(tfb_link_t *link, uint8_t byte);
bool tfb_link_send(tfb_link_t *link, uint8_t *data, size_t size);
bool tfb_link_tx_is_available(tfb_link_t *link);
uint8_t tfb_link_tx_pop_byte(tfb_link_t *link);
void tfb_link_notify_bus_activity(tfb_link_t *link);
int tfb_link_get_timeout(tfb_link_t *link);
void tfb_link_set_tag(tfb_link_t *link, void *tag);
void *tfb_link_get_tag(tfb_link_t *link);
tfb_time_t tfb_link_get_deadline(tfb_link_t *link);
uint32_t tfb_link_get_num_submitted(tfb_link_t *link);
uint32_t tfb_link_get_num_transmitted(tfb_link_t *link);
bool tfb_link_is_transmitted(tfb_link_t *link, uint32_t submitted);
