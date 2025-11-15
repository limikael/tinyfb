#include "tfb_internal.h"
#include <stdio.h>

uint32_t (*tfb_millis)()=NULL;

void tfb_millis_func(uint32_t (*func)()) {
	tfb_millis=func;
}

tfb_time_t tfb_time_now() {
	if (!tfb_millis) {
		printf("**** millis function not set! ****\n");
		return TFB_TIME_NEVER;
	}

	return (tfb_millis()&0x7fffffff);
}

tfb_time_t tfb_time_future(tfb_time_t future_millis) {
    if (future_millis == TFB_TIME_NEVER)
        return TFB_TIME_NEVER;

    return (tfb_time_now() + future_millis) & 0x7FFFFFFF;
}

bool tfb_time_expired(tfb_time_t deadline) {
    if (deadline == TFB_TIME_NEVER)
        return false;   // never expires

    tfb_time_t now = tfb_time_now();
    // Signed delta <= 0 means expired
    return ((int32_t)(deadline - now) <= 0);
}

tfb_time_t tfb_time_soonest(tfb_time_t a, tfb_time_t b) {
    if (a == TFB_TIME_NEVER) return b;
    if (b == TFB_TIME_NEVER) return a;

    return ((int32_t)(a - b) <= 0) ? a : b;
}

int tfb_time_timeout(tfb_time_t deadline) {
    if (deadline == TFB_TIME_NEVER)
        return -1;

    tfb_time_t now = tfb_time_now();
    int32_t diff = (int32_t)(deadline - now);

    if (diff <= 0)
        return 0;

    return diff;
}