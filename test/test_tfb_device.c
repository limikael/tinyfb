#include <stdio.h>
#include "tfb_device.h"
#include "test_helpers.h"
#include "tfb_frame.h"
#include <assert.h>
#include <string.h>

void test_tfb_device_create() {
	printf("- Create and dispose device.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_device_t *device=tfb_device_create(pipe->a,"hello");

	mockpipe_tick(pipe,100);
	tfb_device_tick(device);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"announce_name: (5) 'hello'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t *)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_device_tick(device);
	assert(device->id==123);

	tfb_device_dispose(device);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_device_send() {
	printf("- Device can send, and clear on ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_device_t *device=tfb_device_create(pipe->a,"hello");
	device->id=123;

	mockpipe_tick(pipe,100);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_device_send(device,(uint8_t*)"hello",5);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_device_send(device,(uint8_t*)"world",5);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"payload: (5) 'world'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,8);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_device_tick(device);
	assert(device->tx_committed==8);
	assert(device->tx_pending==2);
	assert(device->tx_buf[0]=='l');

	tfb_device_dispose(device);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_device_recv() {
	printf("- Device can receive, and ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_device_t *device=tfb_device_create(pipe->a,"hello");
	device->id=123;

	mockpipe_tick(pipe,100);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_device_tick(device);

	assert(tfb_device_available(device)==7);
	assert(tfb_device_read_byte(device)=='t');
	assert(tfb_device_read_byte(device)=='e');
	assert(tfb_device_available(device)==5);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"from: 123 ack: 7"));

	tfb_device_dispose(device);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_device_seq_numbers() {
	printf("- Device is flexible about seq numbers.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_device_t *device=tfb_device_create(pipe->a,"hello");
	tfb_frame_t *frame=tfb_frame_create(1024);
	device->id=123;

	mockpipe_tick(pipe,100);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);

	mockpipe_tick(pipe,100);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);
	assert(tfb_device_available(device)==7);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,6);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"gtest",5);
	framesniffer_send_frame(sniffer,frame);
	mockpipe_tick(pipe,100);
	tfb_device_tick(device);
	framesniffer_tick(sniffer);
	//printf("avail: %zu\n",tfb_device_available(device));
	assert(tfb_device_available(device)==11);

	tfb_frame_dispose(frame);
	tfb_device_dispose(device);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}
