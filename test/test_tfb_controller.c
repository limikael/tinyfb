#include <stdio.h>
#include "tfb_controller.h"
#include "test_helpers.h"
#include "tfb_frame.h"
#include <assert.h>
#include <string.h>

void test_tfb_controller_create() {
	printf("- Create and dispose controller, and announce id.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_controller_t *controller=tfb_controller_create(pipe->a);

	mockpipe_tick(pipe,100);
	tfb_controller_tick(controller);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,1000);
	tfb_controller_tick(controller);
	framesniffer_tick(sniffer);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"session_id:"));
	assert(framesniffer_get_num_frames(sniffer)==2);

	tfb_controller_dispose(controller);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_controller_assign() {
	printf("- Controller assigns stream ids.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_controller_t *controller=tfb_controller_create(pipe->a);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_controller_tick(controller);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_data(frame,TFB_ANNOUNCE_NAME,(uint8_t *)"hello",5);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_controller_tick(controller);

	mockpipe_tick(pipe,100);
	tfb_controller_tick(controller);
	framesniffer_tick(sniffer);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"assign_name: (5) 'hello' to:"));
	assert(controller->num_streams==1);
	assert(controller->streams[0]->id==1);

	tfb_controller_dispose(controller);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}
