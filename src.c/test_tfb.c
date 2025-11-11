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

	//printf("allocated: %d\n",tfb_allocated_blocks);
	//assert(tfb_allocated_blocks==0);
}

void test_session_announcement() {
	mock_millis=1000;

	printf("- Send session announcement.\n");
	tfb_t *tfb=tfb_create_controller();
	assert(tfb->session_id!=0);
	assert(tfb->tx_queue_len==1);
	assert(tfb->announcement_deadline==0);

	tfb_tick(tfb);
	assert(!tfb_tx_is_available(tfb));

	mock_millis=2000;
	tfb_tick(tfb);
	assert(tfb_tx_is_available(tfb));

	tfb_frame_t *frame=tfb_frame_create(1024);
	while (tfb_tx_is_available(tfb))
		tfb_frame_rx_push_byte(frame,tfb_tx_pop_byte(tfb));

	assert(tfb->announcement_deadline>0);

	char s[1024];
	tfb_frame_sprint(frame,s);
	//printf("frame: %s\n",s);
	assert(strstr(s,"session_id: "));
	tfb_frame_reset(frame);

	tfb_tick(tfb);
	assert(!tfb_tx_is_available(tfb));

	mock_millis=10000;
	tfb_tick(tfb);
	assert(!tfb->announcement_deadline);
	assert(tfb_tx_is_available(tfb));
	while (tfb_tx_is_available(tfb))
		tfb_frame_rx_push_byte(frame,tfb_tx_pop_byte(tfb));

	assert(tfb->announcement_deadline>0);

	tfb_frame_sprint(frame,s);
	assert(strstr(s,"session_id: "));
	tfb_frame_reset(frame);

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

	while (tfb_frame_tx_is_available(frame))
		tfb_rx_push_byte(tfb,tfb_frame_tx_pop_byte(frame));

	tfb_frame_reset(frame);
	while (tfb_tx_is_available(tfb))
		tfb_frame_rx_push_byte(frame,tfb_tx_pop_byte(tfb));

	char s[1024];
	tfb_frame_sprint(frame,s);
	//printf("frame: %s\n",s);
	assert(strstr(s,"assign_name: (5) 'hello'"));
	tfb_frame_reset(frame);

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
	tfb_frame_t *frame=tfb_frame_create(1024);

	assert(tfb->tx_queue_len==1);
	assert(tfb->announcement_deadline==0);

	tfb_tick(tfb);
	assert(!tfb_tx_is_available(tfb));

	mock_millis=2000;
	tfb_tick(tfb);
	assert(tfb_tx_is_available(tfb));

	tfb_frame_reset(frame);
	while (tfb_tx_is_available(tfb))
		tfb_frame_rx_push_byte(frame,tfb_tx_pop_byte(tfb));

	//assert(tfb->announcement_deadline>0);

	char s[1024];
	tfb_frame_sprint(frame,s);
	//printf("frame: %s\n",s);
	assert(strstr(s,"announce_name: (5) 'hello'"));

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_num(frame,TFB_TO,321);
	tfb_frame_write_data(frame,TFB_ASSIGN_NAME,(uint8_t*)"hello",5);
	tfb_frame_write_checksum(frame);

	while (tfb_frame_tx_is_available(frame))
		tfb_rx_push_byte(tfb,tfb_frame_tx_pop_byte(frame));

	assert(tfb->id==321);
	assert(tfb->session_id==1234);

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

void tfb_read_frame(tfb_t *tfb, tfb_frame_t *frame) {
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
}

void test_device_announcement_on_session() {
	mock_millis=1000;
	printf("- Announce device on receive session.\n");

	tfb_t *tfb=tfb_create_device("hello","world");
	tfb_frame_t *frame=tfb_frame_create(1024);

	mock_millis+=1000;
	tfb_tick(tfb);
	tfb_read_frame(tfb,frame);

	tfb_frame_reset(frame);
	tfb_frame_write_num(frame,TFB_SESSION_ID,1234);
	tfb_frame_write_checksum(frame);
	tfb_write_frame(tfb,frame);

	assert(tfb->tx_queue_len==1);
	assert(tfb->session_id==1234);

	mock_millis+=1000;
	tfb_tick(tfb);
	tfb_read_frame(tfb,frame);
	assert(tfb->tx_queue_len==0);

	assert(!tfb_frame_strcmp(frame,TFB_ANNOUNCE_NAME,"hello"));
	assert(!tfb_frame_strcmp(frame,TFB_ANNOUNCE_TYPE,"world"));

	tfb_frame_dispose(frame);
	tfb_dispose(tfb);
}

int main() {
	//srand((unsigned int)time(NULL));
	srand(0);

	printf("Running tests...\n");
	tfb_millis=mock_millis_func;

	test_tfb_create();
	test_session_announcement();
	test_assign_device();
	test_device_announcement();
	test_device_announcement_on_session();

	printf("Blocks remaining: %d\n",tfb_allocated_blocks);
	assert(!tfb_allocated_blocks);
}