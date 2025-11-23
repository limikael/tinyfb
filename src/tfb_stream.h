#pragma once
#include "tfb_stream.h"
#include "tfb_time.h"
#include "tfb_link.h"

#define TFB_CONNECTION_TIMEOUT 5000
#define TFB_TRANSPORT_BUF_SIZE 256
#define TFB_RESEND_BASE 10
#define TFB_RETRIES 5

typedef struct tfb_stream tfb_stream_t;

struct tfb_stream {
	char *name;
	int id,resend_level,controlled_id;
	tfb_time_t activity_deadline,resend_deadline;
	tfb_link_t *link;
	uint8_t rx_buf[TFB_TRANSPORT_BUF_SIZE], tx_buf[TFB_TRANSPORT_BUF_SIZE];
	size_t rx_pending,rx_committed,tx_pending,tx_committed;
	void (*data_func)(tfb_stream_t *stream);
	void (*status_func)(tfb_stream_t *stream);
	int session_id;
};

tfb_stream_t *tfb_stream_create(tfb_physical_t *physical, char *name);
tfb_stream_t *tfb_stream_create_controlled(tfb_link_t *link, int id, char *name);
void tfb_stream_dispose(tfb_stream_t *stream);
void tfb_stream_tick(tfb_stream_t *stream);
int tfb_stream_send(tfb_stream_t *stream, uint8_t *data, size_t size);
size_t tfb_stream_available(tfb_stream_t *stream);
int tfb_stream_read_byte(tfb_stream_t *stream);
int tfb_stream_recv(tfb_stream_t *stream, uint8_t *data, size_t size);
tfb_time_t tfb_stream_get_deadline(tfb_stream_t *stream);
int tfb_stream_get_timeout(tfb_stream_t *stream);
void tfb_stream_data_func(tfb_stream_t *stream, void (*data_func)(tfb_stream_t *stream));
void tfb_stream_status_func(tfb_stream_t *stream, void (*status_func)(tfb_stream_t *stream));
bool tfb_stream_is_connected(tfb_stream_t *stream);
bool tfb_stream_is_controlled(tfb_stream_t *stream);
