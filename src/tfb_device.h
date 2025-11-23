#pragma once
#include "tfb_device.h"
#include "tfb_time.h"
#include "tfb_link.h"

#define TFB_CONNECTION_TIMEOUT 5000
#define TFB_TRANSPORT_BUF_SIZE 256
#define TFB_RESEND_BASE 10
#define TFB_RETRIES 5

typedef struct tfb_device tfb_device_t;

struct tfb_device {
	char *name;
	int id,resend_level,controlled_id;
	tfb_time_t activity_deadline,resend_deadline;
	tfb_link_t *link;
	uint8_t rx_buf[TFB_TRANSPORT_BUF_SIZE], tx_buf[TFB_TRANSPORT_BUF_SIZE];
	size_t rx_pending,rx_committed,tx_pending,tx_committed;
	void (*data_func)(tfb_device_t *device);
	void (*status_func)(tfb_device_t *device);
	int session_id;
};

tfb_device_t *tfb_device_create(tfb_physical_t *physical, char *name);
tfb_device_t *tfb_device_create_controlled(tfb_link_t *link, int id, char *name);
void tfb_device_dispose(tfb_device_t *device);
void tfb_device_tick(tfb_device_t *device);
int tfb_device_send(tfb_device_t *device, uint8_t *data, size_t size);
size_t tfb_device_available(tfb_device_t *device);
int tfb_device_read_byte(tfb_device_t *device);
int tfb_device_recv(tfb_device_t *device, uint8_t *data, size_t size);
tfb_time_t tfb_device_get_deadline(tfb_device_t *device);
int tfb_device_get_timeout(tfb_device_t *device);
void tfb_device_data_func(tfb_device_t *device, void (*data_func)(tfb_device_t *device));
void tfb_device_status_func(tfb_device_t *device, void (*status_func)(tfb_device_t *device));
bool tfb_device_is_connected(tfb_device_t *device);
bool tfb_device_is_controlled(tfb_device_t *device);
