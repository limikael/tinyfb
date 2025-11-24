#include <stdio.h>
#include <assert.h>
#include "tfb_util.h"

void test_tfb_frame_basic();
void test_mockpipe();
void test_tfb_link_send();
void test_tfb_link_receive();
void test_tfb_stream_create();
void test_tfb_stream_send();
void test_tfb_stream_recv();
void test_tfb_stream_seq_numbers();
void test_tfb_hub_create();
void test_tfb_hub_assign();
void test_tfb_sock_create();

int main() {
	printf("Running tests...\n");

	test_mockpipe();
	test_tfb_frame_basic();
	test_tfb_link_send();
	test_tfb_link_receive();
	test_tfb_stream_create();
	test_tfb_stream_send();
	test_tfb_stream_recv();
	test_tfb_hub_create();
	test_tfb_hub_assign();
	test_tfb_stream_seq_numbers();
	test_tfb_sock_create();

	printf("Blocks remaining: %d\n",tfb_allocated_blocks);
	assert(!tfb_allocated_blocks);
}