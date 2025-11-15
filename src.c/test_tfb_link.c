#include <stdio.h>
#include "tfb_internal.h"
#include <assert.h>
#include <string.h>
#include <time.h>

extern uint32_t mock_millis;

void test_tfb_link() {
	mock_millis=1234;
	printf("- Link can be created, now=%d\n",tfb_time_now());

	tfb_link_t *link=tfb_link_create();

	tfb_link_dispose(link);
}

uint8_t *test_tfb_link_receive_data=NULL;
void test_tfb_link_receive_frame_func(tfb_link_t *t, uint8_t *data, size_t size) {
	if (test_tfb_link_receive_data)
		tfb_free(test_tfb_link_receive_data);

	test_tfb_link_receive_data=tfb_malloc(size+1);
	memcpy(test_tfb_link_receive_data,data,size);
	test_tfb_link_receive_data[size+1]='\0';
}

void test_tfb_link_receive() {
	srand(0);
	mock_millis=1000;
	printf("- Link can receive...\n");

	tfb_link_t *link=tfb_link_create();
	tfb_link_frame_func(link,test_tfb_link_receive_frame_func);
	//printf("available at: %d\n",link->bus_available_deadline);
	assert(link->bus_available_deadline>=1010);
	//assert(tfb_link_get_deadline(link)==1011);

	tfb_link_rx_push_byte(link,'x');
	assert(link->rx_pos==0);

	tfb_link_rx_push_byte(link,0x7e);
	tfb_link_rx_push_byte(link,'x');
	assert(link->rx_pos==1);
	tfb_link_rx_push_byte(link,0x7e);
	assert(test_tfb_link_receive_data==NULL);

	tfb_link_rx_push_byte(link,0x7e);
	tfb_link_rx_push_byte(link,'x');
	tfb_link_rx_push_byte(link,0x7d);
	tfb_link_rx_push_byte(link,0x20^'A');
	tfb_link_rx_push_byte(link,compute_xor_checksum((uint8_t*)"xA",2));
	assert(link->rx_pos==3);

	tfb_link_rx_push_byte(link,0x7e);
	//printf("data: %s\n",(char*)test_tfb_link_receive_data);
	assert(!strcmp((char*)test_tfb_link_receive_data,"xA\x39"));
	assert(link->rx_pos==0);

	tfb_link_dispose(link);

	if (test_tfb_link_receive_data)
		tfb_free(test_tfb_link_receive_data);
}

void test_tfb_link_counters() {
	srand(0);
	mock_millis=1000;
	printf("- Link does counting...\n");

	tfb_link_t *link=tfb_link_create();
	assert(tfb_link_get_num_submitted(link)==0);
	assert(tfb_link_get_num_transmitted(link)==0);

	assert(tfb_link_send(link,(uint8_t*)"hello\x62",6));
	assert(tfb_link_get_num_submitted(link)==1);
	assert(tfb_link_get_num_transmitted(link)==0);

	assert(tfb_link_send(link,(uint8_t*)"hello\x62",6));
	assert(tfb_link_get_num_submitted(link)==2);

	while (tfb_link_tx_is_available(link))
		tfb_link_tx_pop_byte(link);

	assert(tfb_link_get_num_transmitted(link)==0);

	mock_millis+=1000;
	while (tfb_link_tx_is_available(link))
		tfb_link_tx_pop_byte(link);

	assert(tfb_link_is_transmitted(link,1));
	assert(!tfb_link_is_transmitted(link,2));
	assert(tfb_link_get_num_transmitted(link)==1);

	mock_millis+=1000;
	while (tfb_link_tx_is_available(link))
		tfb_link_tx_pop_byte(link);

	assert(tfb_link_get_num_transmitted(link)==2);
	assert(tfb_link_is_transmitted(link,1));
	assert(tfb_link_is_transmitted(link,2));

	tfb_link_dispose(link);
}

void test_tfb_link_send() {
	srand(0);
	mock_millis=1000;
	printf("- Link can send...\n");

	tfb_link_t *link=tfb_link_create();

	assert(!tfb_link_send(link,(uint8_t*)"hello",5));
	assert(!tfb_link_tx_is_available(link));
	assert(tfb_link_send(link,(uint8_t*)"hello\x62",6));
	assert(tfb_link_send(link,(uint8_t*)"h\x7e\x7d\x6b",4));
	assert(link->tx_queue_len==2);

	assert(!tfb_link_tx_is_available(link));
	mock_millis+=100;
	assert(tfb_link_tx_is_available(link));

	uint8_t expected[]={126, 104, 101, 108, 108, 111, 98, 126, 126, 104, 125, 94, 125, 93, 107, 126};
	int count=0;

	while(tfb_link_tx_is_available(link))
		assert(tfb_link_tx_pop_byte(link)==expected[count++]);

	//printf("count=%d\n",count);

	assert(count==8);
	assert(!tfb_link_tx_is_available(link));
	mock_millis+=100;
	assert(tfb_link_tx_is_available(link));

	while(tfb_link_tx_is_available(link))
		assert(tfb_link_tx_pop_byte(link)==expected[count++]);

	assert(count==16);
	assert(!tfb_link_tx_is_available(link));

	tfb_link_dispose(link);
}
