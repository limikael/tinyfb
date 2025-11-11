#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define TFB_CHECKSUM 1
#define TFB_FROM 2
#define TFB_TO 3
#define TFB_PAYLOAD 4
#define TFB_SEQ 5
#define TFB_ACK 6
#define TFB_ANNOUNCE_NAME 7
#define TFB_ANNOUNCE_TYPE 8
#define TFB_ASSIGN_NAME 9
#define TFB_SESSION_ID 10
#define TFB_RESET_TO 11

typedef struct tfb tfb_t;

//tfb_t *tfb_create();
tfb_t *tfb_create_controller();
tfb_t *tfb_create_device(char *name, char *type);
void tfb_dispose(tfb_t *tfb);
void tfb_set_id(tfb_t *tfb, int id);
bool tfb_is_controller(tfb_t *tfb);
bool tfb_is_device(tfb_t *tfb);
bool tfb_is_connected(tfb_t *tfb);
bool tfb_rx_is_available(tfb_t *tfb);
void tfb_rx_push_byte(tfb_t *tfb, uint8_t byte);
void tfb_message_func(tfb_t *tfb, void (*func)(uint8_t *data, size_t size));
void tfb_message_from_func(tfb_t *tfb, void (*func)(uint8_t *data, size_t size, int from));
void tfb_device_func(tfb_t *tfb, void (*func)(char *name));
void tfb_status_func(tfb_t *tfb, void (*func)());
void tfb_millis_func(uint32_t (*func)());
bool tfb_tx_is_available(tfb_t *tfb);
uint8_t tfb_tx_pop_byte(tfb_t *tfb);
bool tfb_send(tfb_t *tfb, uint8_t *data, size_t size);
bool tfb_send_to(tfb_t *t, uint8_t *data, size_t size, int to);
void tfb_tick(tfb_t *tfb);
int tfb_get_queue_len(tfb_t *tfb);
void tfb_notify_bus_activity(tfb_t *tfb);
bool tfb_is_bus_available(tfb_t *tfb);
void tfb_srand(unsigned int seed);
int tfb_get_timeout(tfb_t *tfb);
int tfb_get_device_id_by_name(tfb_t *tfb, char *s);
bool tfb_is_connected(tfb_t *tfb);
