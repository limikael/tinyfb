#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "tfb_link.h"
#include "test_helpers.h"
#include "tfb_util.h"

void test_tfb_link_send() {
	printf("- Link can send...\n");

	mockpipe_t *pipe=mockpipe_create();
	tfb_link_t *link=tfb_link_create(pipe->a);

	assert(!tfb_link_is_bus_available(link));
	mockpipe_tick(pipe,100);
	assert(tfb_link_is_bus_available(link));

	tfb_frame_t *f1=tfb_frame_create(1024);
	tfb_frame_write_num(f1,TFB_TO,123);
	assert(tfb_link_send(link,f1,0));

	assert(pipe->atob_pos==6);

	assert(tfb_link_send(link,f1,TFB_LINK_SEND_OWNED));
	assert(link->tx_frame);
	assert(pipe->atob_pos==6);
	mockpipe_tick(pipe,100);
	tfb_link_tick(link);
	assert(pipe->atob_pos==12);

	/*for (int i=0; i<pipe->atob_pos; i++)
		printf("%02x ",pipe->atob[i]);
	
	printf("(%d)\n",pipe->atob_pos);*/

	tfb_link_dispose(link);
	mockpipe_dispose(pipe);
}

void test_tfb_link_receive() {
	srand(0);
	printf("- Link can receive...\n");

	mockpipe_t *pipe=mockpipe_create();
	tfb_link_t *link=tfb_link_create(pipe->a);

	assert(link->bus_available_deadline>=1001);
	//assert(tfb_link_get_deadline(link)==1011);

	tfb_physical_write(pipe->b,0x7e);
	tfb_physical_write(pipe->b,'x');
	tfb_physical_write(pipe->b,0x7d);
	tfb_physical_write(pipe->b,0x20^'A');
	tfb_physical_write(pipe->b,compute_xor_checksum((uint8_t*)"xA",2));
	tfb_physical_write(pipe->b,0x7e);

	tfb_link_tick(link);
	assert(tfb_link_peek(link));
	tfb_link_consume(link);
	assert(!tfb_link_peek(link));

	tfb_link_dispose(link);
	mockpipe_dispose(pipe);
}
