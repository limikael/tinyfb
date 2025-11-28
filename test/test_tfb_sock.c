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

	//mockpipe_tick(pipe,100);
	//printf("btoa pos: %d\n",pipe->btoa_pos);

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

	assert(strstr(framesniffer_sprint_last(sniffer),"payload: (5) 'hello'"));

	tfb_frame_t *f;

	f=tfb_frame_create(1024);
	tfb_frame_write_num(f,TFB_TO,123);
	tfb_frame_write_num(f,TFB_ACK,55);
	framesniffer_send_frame(sniffer,f);
	tfb_sock_tick(sock);

	f=tfb_frame_create(1024);
	tfb_frame_write_num(f,TFB_TO,123);
	tfb_frame_write_num(f,TFB_ACK,0);
	framesniffer_send_frame(sniffer,f);
	tfb_sock_tick(sock);
	assert(sock->tx_queue_len==0);

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_sock_recv() {
	printf("- sock can receive, and ack.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	sock->id=123;

	mockpipe_tick(pipe,100);
	tfb_sock_tick(sock);
	framesniffer_tick(sniffer);

	tfb_frame_t *f=tfb_frame_create(1024);
	tfb_frame_write_num(f,TFB_TO,123);
	tfb_frame_write_num(f,TFB_SEQ,0);
	tfb_frame_write_data(f,TFB_PAYLOAD,(uint8_t*)"testing",7);
	framesniffer_send_frame(sniffer,f);

	mockpipe_tick(pipe,100);
	framesniffer_tick(sniffer);
	tfb_sock_tick(sock);

	assert(tfb_sock_available(sock)==7);
	uint8_t buf[2];
	tfb_sock_recv(sock,buf,2);
	assert(buf[0]=='t');
	assert(buf[1]=='e');
	assert(tfb_sock_available(sock)==0);

	//for (int i=0; i<framesniffer_get_num_frames(sniffer); i++)
	//	printf("%s\n",framesniffer_sprint_frame_at(sniffer,i));

	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}

void test_tfb_sock_resend() {
	printf("- Sock resends properly.\n");

	mockpipe_t *pipe=mockpipe_create();
	framesniffer_t *sniffer=framesniffer_create(pipe->b);
	tfb_sock_t *sock=tfb_sock_create(pipe->a,"hello");
	tfb_frame_t *frame=tfb_frame_create(1024);
	sock->id=123;

	//assert(sock->resend_deadline==TFB_TIME_NEVER);
	tfb_sock_send(sock,(uint8_t *)"hello",5);
	uint32_t sendnum=tfb_link_get_num_sent(sock->link);
	assert(sendnum==1);
	assert(!tfb_link_is_transmitted(sock->link,sendnum));
	assert(sock->resend_deadline==TFB_TIME_NEVER);

	mockpipe_tick(pipe,4);
	tfb_sock_tick(sock);
	assert(tfb_link_is_transmitted(sock->link,sendnum));
	assert(sock->resend_deadline!=TFB_TIME_NEVER);

	tfb_frame_dispose(frame);
	tfb_sock_dispose(sock);
	framesniffer_dispose(sniffer);
	mockpipe_dispose(pipe);
}
