#include "tfb_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tfb_physical.h"

tfb_time_t tfb_time_now(tfb_physical_t *physical) {
	return (tfb_physical_millis(physical)&0x7fffffff);
}

tfb_time_t tfb_time_future(tfb_physical_t *physical, tfb_time_t future_millis) {
    if (future_millis == TFB_TIME_NEVER)
        return TFB_TIME_NEVER;

    return (tfb_time_now(physical) + future_millis) & 0x7FFFFFFF;
}

bool tfb_time_expired(tfb_physical_t *physical, tfb_time_t deadline) {
    if (deadline == TFB_TIME_NEVER)
        return false;   // never expires

    tfb_time_t now = tfb_time_now(physical);
    // Signed delta <= 0 means expired
    return ((int32_t)(deadline - now) <= 0);
}

tfb_time_t tfb_time_soonest(tfb_time_t a, tfb_time_t b) {
    if (a == TFB_TIME_NEVER) return b;
    if (b == TFB_TIME_NEVER) return a;

    return ((int32_t)(a - b) <= 0) ? a : b;
}

int tfb_time_timeout(tfb_physical_t *physical, tfb_time_t deadline) {
    if (deadline == TFB_TIME_NEVER)
        return -1;

    tfb_time_t now = tfb_time_now(physical);
    int32_t diff = (int32_t)(deadline - now);

    if (diff <= 0)
        return 0;

    return diff;
}
