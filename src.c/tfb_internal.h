#pragma once

#include <stdint.h>

typedef uint32_t tfb_time_t;

#include "tfb.h"
#include "tfb_frame.h"
#include "tfb_util.h"
#include "tfb_device.h"
#include "tfb_link.h"
#include "tfb_time.h"

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

#define TFB_RX_BUF_SIZE 1024
#define TFB_RESEND_BASE 20
#define TFB_RETRIES 5
#define TFB_ANNOUNCEMENT_INTERVAL 1000
#define TFB_CONNECTION_TIMEOUT 5000
#define TFB_LINK_QUEUE_LEN 8
#define TFB_TRANSPORT_QUEUE_LEN 8

#define TFB_TIME_NEVER 0xffffffff

#define TFB_CA_DELAY_MIN 5
#define TFB_CA_DELAY_MAX 10

typedef enum {
	TFB_LINK_RX_INIT,
	TFB_LINK_RX_RECEIVE,
	TFB_LINK_RX_ESCAPE
} tfb_link_rx_state_t;

typedef enum {
	TFB_LINK_TX_IDLE,
	TFB_LINK_TX_SENDING,
	TFB_LINK_TX_ESCAPE
} tfb_link_tx_state_t;

struct tfb_link {
	uint8_t *rx_buf;
	uint32_t rx_pos,tx_queue_len,tx_index;
	tfb_link_rx_state_t rx_state;
	tfb_link_tx_state_t tx_state;
	void (*frame_func)(tfb_link_t *link, uint8_t *data, size_t size);
	tfb_time_t bus_available_deadline;
	void *tag;
	uint32_t num_submitted;

	struct {
		uint8_t *data;
		size_t size;
	} tx_queue[TFB_LINK_QUEUE_LEN];
};

struct tfb_device {
	char *name,*type;
	int id;
	tfb_time_t activity_deadline;
	int inseq,outseq;
};

struct tfb {
	tfb_link_t *link;
	int id, outseq, inseq;
	char *device_name,*device_type;
	tfb_time_t announcement_deadline,activity_deadline;
	void (*message_func)(uint8_t *data, size_t size);
	void (*message_from_func)(uint8_t *data, size_t size, int from);
	void (*device_func)(char *name);
	void (*status_func)();
	tfb_device_t **devices;
	size_t num_devices;
	int session_id;
	int errno;
	tfb_frame_t *tx_queue[TFB_TRANSPORT_QUEUE_LEN];
	size_t tx_queue_len;
};

struct tfb_frame {
	uint8_t *buffer;
	size_t size, capacity;
	int resend_count;
	tfb_time_t deadline;
	uint32_t submission_number;
};
