#include "tfb_util.h"
#include "test_helpers.h"
#include "tfb_physical.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

size_t mockpipe_available_a(tfb_physical_t *physical) {
	mockpipe_t *pipe=physical->tag;
	return pipe->btoa_pos;
}

int mockpipe_read_a(tfb_physical_t *physical) {
	mockpipe_t *pipe=physical->tag;
	if (!pipe->btoa_pos)
		return -1;

	uint8_t byte=pipe->btoa[0];
	memmove(pipe->btoa,pipe->btoa+1,1023);
	pipe->btoa_pos--;

	return byte;
}

size_t mockpipe_write_a(tfb_physical_t *physical, uint8_t byte) {
	mockpipe_t *pipe=physical->tag;
	pipe->atob[pipe->atob_pos++]=byte;

	return 1;
}

size_t mockpipe_available_b(tfb_physical_t *physical) {
	mockpipe_t *pipe=physical->tag;
	return pipe->atob_pos;
}

int mockpipe_read_b(tfb_physical_t *physical) {
	mockpipe_t *pipe=physical->tag;
	if (!pipe->atob_pos)
		return -1;

	uint8_t byte=pipe->atob[0];
	memmove(pipe->atob,pipe->atob+1,1023);
	pipe->atob_pos--;

	return byte;
}

size_t mockpipe_write_b(tfb_physical_t *physical, uint8_t byte) {
	mockpipe_t *pipe=physical->tag;
	pipe->btoa[pipe->btoa_pos++]=byte;

	return 1;
}

uint32_t mockpipe_millis(tfb_physical_t *physical) {
	mockpipe_t *pipe=physical->tag;
	return pipe->millis;
}

mockpipe_t *mockpipe_create() {
	mockpipe_t *pipe=tfb_malloc(sizeof(mockpipe_t));

	pipe->atob_pos=0;
	pipe->btoa_pos=0;
	pipe->millis=1000;

	pipe->a=tfb_physical_create();
	pipe->a->tag=pipe;
	pipe->a->available=mockpipe_available_a;
	pipe->a->read=mockpipe_read_a;
	pipe->a->write=mockpipe_write_a;
	pipe->a->millis=mockpipe_millis;

	pipe->b=tfb_physical_create();
	pipe->b->tag=pipe;
	pipe->b->available=mockpipe_available_b;
	pipe->b->read=mockpipe_read_b;
	pipe->b->write=mockpipe_write_b;
	pipe->b->millis=mockpipe_millis;

	return pipe;
}

void mockpipe_dispose(mockpipe_t *pipe) {
	tfb_physical_dispose(pipe->a);
	tfb_physical_dispose(pipe->b);

	tfb_free(pipe);
}

void mockpipe_tick(mockpipe_t *pipe, uint32_t millis) {
	pipe->millis+=millis;
}

framesniffer_t *framesniffer_create(tfb_physical_t *physical) {
	framesniffer_t *sniffer=tfb_malloc(sizeof(framesniffer_t));
	sniffer->link=tfb_link_create(physical);
	sniffer->num_frames=0;

	return sniffer;
}

void framesniffer_dispose(framesniffer_t *sniffer) {
	for (int i=0; i<sniffer->num_frames; i++)
		tfb_frame_dispose(sniffer->frames[i]);

	tfb_link_dispose(sniffer->link);
	tfb_free(sniffer);
}

void framesniffer_tick(framesniffer_t *sniffer) {
	tfb_link_tick(sniffer->link);

	if (tfb_link_peek_size(sniffer->link)) {
		uint8_t *data=tfb_link_peek(sniffer->link);
		size_t size=tfb_link_peek_size(sniffer->link);
		sniffer->frames[sniffer->num_frames++]=tfb_frame_create_from_data(data,size);
		tfb_link_consume(sniffer->link);
	}
}

void framesniffer_send_frame(framesniffer_t *sniffer, tfb_frame_t *frame) {
	framesniffer_tick(sniffer);

	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	tfb_link_send(sniffer->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),TFB_LINK_URGENT);

	framesniffer_tick(sniffer);
}

char *framesniffer_sprint_frame_at(framesniffer_t *sniffer, int n) {
	for (int i=0; i<3; i++)
		framesniffer_tick(sniffer);

	static char s[1024];
	//char *s=malloc(1024);

	tfb_frame_sprint(sniffer->frames[n],s);

	return s;
}

char *framesniffer_sprint_last(framesniffer_t *sniffer) {
	return framesniffer_sprint_frame_at(sniffer,framesniffer_get_num_frames(sniffer)-1);
}


int framesniffer_get_num_frames(framesniffer_t *sniffer) {
	for (int i=0; i<3; i++)
		framesniffer_tick(sniffer);

	return sniffer->num_frames;	
}

void test_mockpipe() {
	printf("- Mock pipe.\n");
	mockpipe_t *pipe=mockpipe_create();

	assert(pipe->atob_pos==0);
	pipe->a->write(pipe->a,111);
	pipe->a->write(pipe->a,222);
	assert(pipe->atob_pos==2);

	assert(pipe->b->read(pipe->b)==111);
	assert(pipe->b->read(pipe->b)==222);

	mockpipe_dispose(pipe);
}
