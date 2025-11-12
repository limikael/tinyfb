#pragma once

#include "tfb.h"
#include "tfb_frame.h"
#include "tfb_util.h"
#include "tfb_device.h"

#define TFB_RX_BUF_SIZE 1024
#define TFB_TX_QUEUE_LEN 16
#define TFB_RESEND_BASE 5
#define TFB_RETRIES 5
#define TFB_ANNOUNCEMENT_INTERVAL 1000
#define TFB_CONNECTION_TIMEOUT 5000

extern uint32_t (*tfb_millis)();

typedef enum {
	RX_RECEIVE,
	RX_ESCAPE,
	RX_COMPLETE,
} tfb_frame_rx_state_t;

typedef enum {
	TX_INIT,
	TX_SENDING,
	TX_ESCAPING,
	TX_COMPLETE
} tfb_frame_tx_state_t;

struct tfb_device {
	char *name,*type;
	int id;
	uint32_t activity_deadline;
};

struct tfb {
	tfb_frame_t *rx_frame,*tx_frame;
	tfb_frame_t *tx_queue[TFB_TX_QUEUE_LEN];
	size_t tx_queue_len;
	bool rx_deliverable;
	int id, seq;
	uint32_t bus_available_millis;
	char *device_name,*device_type;
	uint32_t announcement_deadline,activity_deadline;
	void (*message_func)(uint8_t *data, size_t size);
	void (*message_from_func)(uint8_t *data, size_t size, int from);
	void (*device_func)(char *name);
	void (*status_func)();
	tfb_device_t **devices;
	size_t num_devices;
	int session_id;
};

struct tfb_frame {
	uint8_t *buffer;
	size_t size, capacity, tx_index;
	tfb_frame_rx_state_t rx_state;
	tfb_frame_tx_state_t tx_state;
	tfb_t *tfb;
	void (*notification_func)(tfb_t *tfb, tfb_frame_t *tfb_frame);
	int send_at,sent_times;
};
