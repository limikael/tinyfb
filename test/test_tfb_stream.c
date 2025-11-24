#include <stdio.h>
#include "tfb_stream.h"
#include "test_helpers.h"
#include "tfb_frame.h"
#include <assert.h>
#include <string.h>

void test_tfb_stream_create() {
	printf("- Create and dispose stream.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_stream_t *stream=tfb_stream_create(pipe->a,"hello");

	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"announce_name: (5) 'hello'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t *)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_stream_tick(stream);
	assert(stream->id==123);

	tfb_stream_dispose(stream);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_stream_send() {
	printf("- Stream can send, and clear on ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_stream_t *stream=tfb_stream_create(pipe->a,"hello");
	stream->id=123;

	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_stream_send(stream,(uint8_t*)"hello",5);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_stream_send(stream,(uint8_t*)"world",5);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);

	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	assert(strstr(framesniffer_sprint_last(sniffer),"payload: (5) 'world'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,8);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_stream_tick(stream);
	assert(stream->tx_committed==8);
	assert(stream->tx_pending==2);
	assert(stream->tx_buf[0]=='l');

	tfb_stream_dispose(stream);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_stream_recv() {
	printf("- Stream can receive, and ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_stream_t *stream=tfb_stream_create(pipe->a,"hello");
	stream->id=123;

	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_stream_tick(stream);

	assert(tfb_stream_available(stream)==7);
	uint8_t buf[2];
	tfb_stream_recv(stream,buf,2);
	assert(buf[0]=='t');
	assert(buf[1]=='e');
	assert(tfb_stream_available(stream)==5);

	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	assert(strstr(framesniffer_sprint_last(sniffer),"from: 123 ack: 7"));

	tfb_stream_dispose(stream);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_stream_seq_numbers() {
	printf("- Stream is flexible about seq numbers.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_stream_t *stream=tfb_stream_create(pipe->a,"hello");
	tfb_frame_t *frame=tfb_frame_create(1024);
	stream->id=123;

	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);

	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);
	assert(tfb_stream_available(stream)==7);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,6);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"gtest",5);
	framesniffer_send_frame(sniffer,frame);
	mockpipe_tick(pipe,100);
	tfb_stream_tick(stream);
	framesniffer_tick(sniffer);
	//printf("avail: %zu\n",tfb_stream_available(stream));
	assert(tfb_stream_available(stream)==11);

	tfb_frame_dispose(frame);
	tfb_stream_dispose(stream);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}
