#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tfb_link.h"
#include "tfb_time.h"
#include "tfb_stream.h"

#define TFB_ENOTCONN 1
#define TFB_ENOSPC 2
#define TFB_EOPNOTSUPP 3

#define TFB_ANNOUNCEMENT_INTERVAL 1000

typedef struct tfb_controller tfb_controller_t;

struct tfb_controller {
	tfb_link_t *link;
	int session_id;
	tfb_time_t announcement_deadline;
	tfb_stream_t *streams[16];
	size_t num_streams;
	void (*stream_func)(tfb_controller_t *controller, tfb_stream_t *stream);
};

tfb_controller_t *tfb_controller_create(tfb_physical_t *physical);
void tfb_controller_dispose(tfb_controller_t *controller);
void tfb_controller_tick(tfb_controller_t *controller);
tfb_time_t tfb_controller_get_deadline(tfb_controller_t *controller);
int tfb_controller_get_timeout(tfb_controller_t *controller);
void tfb_controller_stream_func(tfb_controller_t *controller, void (*stream_func)(tfb_controller_t *controller, tfb_stream_t *stream));
