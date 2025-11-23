#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tfb_link.h"
#include "tfb_time.h"
#include "tfb_device.h"

#define TFB_ENOTCONN 1
#define TFB_ENOSPC 2
#define TFB_EOPNOTSUPP 3

#define TFB_ANNOUNCEMENT_INTERVAL 1000

typedef struct tfb_controller tfb_controller_t;

struct tfb_controller {
	tfb_link_t *link;
	int session_id;
	tfb_time_t announcement_deadline;
	tfb_device_t *devices[16];
	size_t num_devices;
	void (*device_func)(tfb_controller_t *controller, tfb_device_t *device);
};

tfb_controller_t *tfb_controller_create(tfb_physical_t *physical);
void tfb_controller_dispose(tfb_controller_t *controller);
void tfb_controller_tick(tfb_controller_t *controller);
tfb_time_t tfb_controller_get_deadline(tfb_controller_t *controller);
int tfb_controller_get_timeout(tfb_controller_t *controller);
void tfb_controller_device_func(tfb_controller_t *controller, void (*device_func)(tfb_controller_t *controller, tfb_device_t *device));
