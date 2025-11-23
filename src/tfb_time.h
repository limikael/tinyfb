#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "tfb_physical.h"

#define TFB_TIME_NEVER 0xffffffff

typedef uint32_t tfb_time_t;

tfb_time_t tfb_time_now(tfb_physical_t *physical);
tfb_time_t tfb_time_future(tfb_physical_t *physical, tfb_time_t future_millis);
bool tfb_time_expired(tfb_physical_t *physical, tfb_time_t deadline);
tfb_time_t tfb_time_soonest(tfb_time_t a, tfb_time_t b);
int tfb_time_timeout(tfb_physical_t *physical, tfb_time_t deadline);
