#include <stdio.h>
#include "tfb.h"
#include "tfb_internal.h"
#include <assert.h>
#include <time.h>
#include <string.h>

uint32_t mock_millis=1000;
uint32_t mock_millis_func() {
	return mock_millis;
}

void test_tfb_create() {
	printf("- Create and dispose.\n");
	tfb_t *tfb=tfb_create_controller();
	assert(tfb!=NULL);
	tfb_dispose(tfb);
}

void test_session_announcement() {
	mock_millis=1000;

	printf("- Send session announcement.\n");
	tfb_t *tfb=tfb_create_controller();
	assert(tfb->session_id!=0);
	assert(tfb->announcement_deadline>0);

	tfb_tick(tfb);
	assert(tfb->link->tx_queue_len==1);

	mock_millis=2000;
	tfb_tick(tfb);
	assert(tfb->link->tx_queue_len==2);

	tfb_frame_t *frame=tfb_frame_create_from_data(tfb->link->tx_queue[0].data,tfb->link->tx_queue[0].size);
	char s[1024];
	tfb_frame_sprint(frame,s);
	//printf("frame: %s\n",s);
	assert(strstr(s,"session_id: 19040"));

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

char *device_listener_param=NULL;
void device_listener(char *s) {
	device_listener_param=s;
}

void test_assign_device() {
	mock_millis=1000;

	printf("- Assign device id.\n");
	tfb_t *tfb=tfb_create_controller();
	tfb_device_func(tfb,device_listener);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_data(frame,TFB_ANNOUNCE_NAME,(uint8_t *)"hello",5);
	tfb_frame_write_data(frame,TFB_ANNOUNCE_TYPE,(uint8_t *)"type",4);
	tfb_frame_write_checksum(frame);

	mock_millis+=500;
	tfb_tick(tfb);

	tfb_handle_frame(tfb,frame->buffer,frame->size);

	mock_millis+=500;
	tfb_tick(tfb);

	//printf("q: %d\n",tfb->link->tx_queue_len);
	assert(tfb->link->tx_queue_len==2);

	tfb_frame_t *f2=tfb_frame_create_from_data(tfb->link->tx_queue[1].data,tfb->link->tx_queue[1].size);
	char s[1024];
	tfb_frame_sprint(f2,s);
	//printf("frame: %s\n",s);
	//assert(strstr(s,"assign_name: (5) 'hello'"));
	tfb_frame_dispose(f2);

	assert(tfb_get_device_id_by_name(tfb,"hello")==1);
	assert(tfb_get_device_id_by_name(tfb,"hello2")==0);
	assert(!strcmp(device_listener_param,"hello"));

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

void test_device_announcement() {
	mock_millis=1000;

	printf("- Announce device.\n");
	tfb_t *tfb=tfb_create_device("hello","world");

	assert(tfb->link->tx_queue_len==1);
	assert(tfb->announcement_deadline==TFB_TIME_NEVER);

	tfb_frame_t *frame=tfb_frame_create(1024);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_num(frame,TFB_TO,321);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t*)"hello",5);
	tfb_frame_write_checksum(frame);

	tfb_handle_frame(tfb,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame));

	assert(tfb->id==321);
	assert(tfb->session_id==1234);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

/*void tfb_read_frame(tfb_t *tfb, tfb_frame_t *frame) {
	assert(tfb_tx_is_available(tfb));
	tfb_frame_reset(frame);
	while (tfb_tx_is_available(tfb))
		tfb_frame_rx_push_byte(frame,tfb_tx_pop_byte(tfb));

	assert(tfb_frame_rx_is_complete(frame));
}

void tfb_write_frame(tfb_t *tfb, tfb_frame_t *frame) {
	assert(tfb_rx_is_available(tfb));
	while (tfb_frame_tx_is_available(frame))
		tfb_rx_push_byte(tfb,tfb_frame_tx_pop_byte(frame));
}*/

void test_device_announcement_on_session() {
	mock_millis=1000;
	printf("- Announce device on receive session.\n");

	tfb_t *tfb=tfb_create_device("hello","world");
	tfb_frame_t *frame=tfb_frame_create(1024);
	mock_millis+=1000;
	tfb_tick(tfb);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame));

	assert(tfb->link->tx_queue_len==2);
	assert(tfb->session_id==1234);

	tfb_frame_t *f2=tfb_frame_create_from_data(tfb->link->tx_queue[1].data,tfb->link->tx_queue[1].size);
	assert(!tfb_frame_strcmp(f2,TFB_ANNOUNCE_NAME,"hello"));
	assert(!tfb_frame_strcmp(f2,TFB_ANNOUNCE_TYPE,"world"));
	tfb_frame_dispose(f2);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

int test_receive_assignment_status_calls=0;
void test_receive_assignment_status() {
	test_receive_assignment_status_calls++;
}

void test_receive_assignment() {
	printf("- Receive and drop assignment.\n");
	mock_millis=1000;
	test_receive_assignment_status_calls=0;
	tfb_t *tfb=tfb_create_device("hello","world");
	tfb_status_func(tfb,test_receive_assignment_status);
	tfb_frame_t *frame=tfb_frame_create(1024);

	mock_millis+=1000;
	tfb_tick(tfb);

	while (tfb_tx_is_available(tfb))
		tfb_tx_pop_byte(tfb);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t*)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);

	assert(tfb->session_id==1234);
	assert(tfb->id==123);
	assert(tfb_is_connected(tfb));
	assert(test_receive_assignment_status_calls==1);

	mock_millis+=1000;
	tfb_tick(tfb);
	assert(!tfb_tx_is_available(tfb));

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_num(frame,TFB_RESET_TO,123);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);

	assert(tfb->session_id==1234);
	assert(tfb->id==-1);
	assert(!tfb_is_connected(tfb));
	assert(test_receive_assignment_status_calls==2);

	mock_millis+=1000;
	tfb_tick(tfb);

	tfb_frame_t *f2=tfb_frame_create_from_data(
		tfb->link->tx_queue[tfb->link->tx_queue_len-1].data,
		tfb->link->tx_queue[tfb->link->tx_queue_len-1].size
	);

	char s[1024];
	tfb_frame_sprint(f2,s);
	//printf("frame: %s\n",tfb_frame_sprint(f2,s));
	assert(!strcmp(s,"announce_name: (5) 'hello' announce_type: (5) 'world' checksum: 113 "));
	tfb_frame_dispose(f2);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

void test_tfb_send() {
	printf("- Sends, and clears the queue on ack.\n");
	mock_millis=1000;
	tfb_t *tfb=tfb_create_device("hello","world");
	tfb_frame_t *frame=tfb_frame_create(1024);

	assert(!tfb_send(tfb,(uint8_t*)"hello",5));

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t*)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);

	assert(tfb_send(tfb,(uint8_t*)"hello",5));
	assert(tfb_send(tfb,(uint8_t*)"hello again",11));
	assert(tfb->tx_queue_len==2);
	assert(tfb_frame_get_num(tfb->tx_queue[0],TFB_SEQ)==1);
	assert(tfb_frame_get_num(tfb->tx_queue[1],TFB_SEQ)==2);
	assert(tfb->outseq==3);

	tfb_frame_t *f2=tfb_frame_create_from_data(
		tfb->link->tx_queue[tfb->link->tx_queue_len-1].data,
		tfb->link->tx_queue[tfb->link->tx_queue_len-1].size
	);

	char s[1024];
	tfb_frame_sprint(f2,s);
	//printf("frame: %s\n",tfb_frame_sprint(f2,s));
	assert(strstr(s,"payload: (11) 'hello again'"));
	tfb_frame_dispose(f2);

	//mock_millis+=1000;

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,999);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);
	assert(tfb->tx_queue_len==2);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,1);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);
	assert(tfb->tx_queue_len==1);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_num(frame,TFB_ACK,2);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);
	assert(tfb->tx_queue_len==0);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

void test_tfb_resend() {
	printf("- Resends.\n");
	mock_millis=1000;
	tfb_t *tfb=tfb_create_device("hello","world");
	tfb_frame_t *frame=tfb_frame_create(1024);

	assert(!tfb_send(tfb,(uint8_t*)"hello",5));

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t*)"hello",5);
	tfb_frame_write_num(frame,TFB_TO,123);
	tfb_frame_write_checksum(frame);
	tfb_handle_frame(tfb,frame->buffer,frame->size);

	assert(tfb_send(tfb,(uint8_t*)"hello",5));

	for (int i=0; i<10; i++) {
		mock_millis+=1000;
		tfb_tick(tfb);
	}

	//printf("queue len: %d\n",tfb->link->tx_queue_len);
	assert(tfb->link->tx_queue_len==7);
	assert(tfb->tx_queue_len==0);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

void test_tfb_link();
void test_tfb_link_receive();
void test_tfb_link_send();

void test_tfb_frame_basic();

int main() {
	//srand((unsigned int)time(NULL));
	srand(0);

	printf("Running tests...\n");
	tfb_millis_func(mock_millis_func);

	test_tfb_link();
	test_tfb_link_receive();
	test_tfb_link_send();

	test_tfb_create();
	test_session_announcement();
	test_assign_device();
	test_device_announcement();
	test_device_announcement_on_session();
	test_receive_assignment();
	test_tfb_send();
	test_tfb_resend();

	test_tfb_frame_basic();

	printf("Blocks remaining: %d\n",tfb_allocated_blocks);
	assert(!tfb_allocated_blocks);
}