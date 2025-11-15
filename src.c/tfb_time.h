#pragma once
#include "tfb_internal.h"

typedef uint32_t tfb_time_t;

tfb_time_t tfb_time_now();
tfb_time_t tfb_time_future(tfb_time_t future_millis);
bool tfb_time_expired(tfb_time_t deadline);
tfb_time_t tfb_time_soonest(tfb_time_t a, tfb_time_t b);
int tfb_time_timeout(tfb_time_t deadline);
