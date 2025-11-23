#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "tfb_frame.h"

void test_tfb_frame_basic() {
	printf("- Frame.\n");

	tfb_frame_t *frame=tfb_frame_create(1024);

	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_FROM,456);

	assert(tfb_frame_get_num(frame,TFB_TO)==123);
	assert(tfb_frame_get_num(frame,TFB_FROM)==456);

	tfb_frame_dispose(frame);
}
