#pragma once

#include "tfb_physical.h"
#include "tfb_link.h"
#include "tfb_frame.h"

typedef struct mockpipe mockpipe_t;

struct mockpipe {
	tfb_physical_t *a,*b;
	uint8_t atob[1024],btoa[1024];
	int atob_pos,btoa_pos;
	uint32_t millis;
};

typedef struct framesniffer framesniffer_t;

struct framesniffer {
	tfb_link_t *link;
	int num_frames;
	tfb_frame_t *frames[32];
};

mockpipe_t *mockpipe_create();
void mockpipe_dispose(mockpipe_t *pipe);
void mockpipe_tick(mockpipe_t *pipe, uint32_t millis);

framesniffer_t *framesniffer_create(tfb_physical_t *physical);
void framesniffer_dispose(framesniffer_t *sniffer);
void framesniffer_tick(framesniffer_t *sniffer);
char *framesniffer_sprint_frame_at(framesniffer_t *sniffer, int i);
char *framesniffer_sprint_last(framesniffer_t *sniffer);
int framesniffer_get_num_frames(framesniffer_t *sniffer);
void framesniffer_send_frame(framesniffer_t *sniffer, tfb_frame_t *frame);
