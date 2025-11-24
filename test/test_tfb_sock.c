#include <stdio.h>
#include "tfb_sock.h"
#include "test_helpers.h"
#include "tfb_frame.h"
#include <assert.h>
#include <string.h>

void test_tfb_sock_create() {
	printf("- Create and dispose sock.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);

	/*for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
		printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));*/

	assert(strstr(framesniffer_sprint_last(sniffer),"announce_name: (5) 'hello'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t *)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_sock_tick(sock);
	assert(sock->id==123);

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_sock_send() {
	printf("- sock can send, and clear on ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	sock->id=123;

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_sock_send(sock,(uint8_t*)"hello",5);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	tfb_sock_send(sock,(uint8_t*)"world",5);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	assert(strstr(framesniffer_sprint_last(sniffer),"payload: (5) 'world'"));

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,8);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	tfb_sock_tick(sock);
	assert(sock->tx_committed==8);
	assert(sock->tx_pending==2);
	assert(sock->tx_buf[0]=='l');

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

/*void test_tfb_sock_send_sock() {
	printf("- sock can send and actually sock.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	sock->id=123;

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	mockpipe_tick(pipe,100);
	int sent1=tfb_sock_send(sock,(uint8_t*)"01234567890123456789012345678901234567890123456789012345678901234567890123456789",80);
	//printf("sent bytes: %d, pending: %zu, dl: %u\n",sent1, sock->tx_pending,tfb_sock_get_deadline(sock));

	int sent2=tfb_sock_send(sock,(uint8_t*)"01234567890123456789012345678901234567890123456789012345678901234567890123456789",80);
	//printf("sent bytes: %d\n",sent2);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,54);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	mockpipe_tick(pipe,10);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	//printf("pending after: %zu, dl after: %u\n",sock->tx_pending,tfb_sock_get_deadline(sock));
	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}*/

void test_tfb_sock_recv() {
	printf("- sock can receive, and ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	sock->id=123;

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);
	tfb_frame_dispose(frame);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_sock_tick(sock);

	assert(tfb_sock_available(sock)==7);
	uint8_t buf[2];
	tfb_sock_recv(sock,buf,2);
	assert(buf[0]=='t');
	assert(buf[1]=='e');
	assert(tfb_sock_available(sock)==5);

	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	assert(strstr(framesniffer_sprint_last(sniffer),"from: 123 ack: 7"));

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_sock_seq_numbers() {
	printf("- sock is flexible about seq numbers.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	tfb_frame_t *frame=tfb_frame_create(1024);
	sock->id=123;

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,0);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,frame);

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);
	assert(tfb_sock_available(sock)==7);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_SEQ,6);
	tfb_frame_write_data(frame,TFB_PAYLOAD,(uint8_t*)"gtest",5);
	framesniffer_send_frame(sniffer,frame);
	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);
	//printf("avail: %zu\n",tfb_sock_available(sock));
	assert(tfb_sock_available(sock)==11);

	tfb_frame_dispose(frame);
	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}
