#include "tfb_controller.h"
#include "tfb_link.h"
#include "tfb_util.h"
#include "tfb_frame.h"
#include "tfb_time.h"
#include <stdio.h>
#include <string.h>

void tfb_controller_stream_func(tfb_controller_t *controller, void (*stream_func)(tfb_controller_t *controller, tfb_stream_t *stream)) {
	controller->stream_func=stream_func;
}

tfb_controller_t *tfb_controller_create(tfb_physical_t *physical) {
	tfb_controller_t *controller=tfb_malloc(sizeof(tfb_controller_t));

	controller->link=tfb_link_create(physical);
	controller->num_streams=0;
	controller->session_id=(rand()%32000)+1;
	controller->announcement_deadline=tfb_time_now(physical);
	controller->stream_func=NULL;

	return controller;
}

void tfb_controller_dispose(tfb_controller_t *controller) {
	for (int i=0; i<controller->num_streams; i++)
		tfb_stream_dispose(controller->streams[i]);

	tfb_link_dispose(controller->link);
	tfb_free(controller);
}

tfb_stream_t *tfb_controller_get_stream_by_name(tfb_controller_t *controller, char *name) {
	for (int i=0; i<controller->num_streams; i++)
		if (!strcmp(name,controller->streams[i]->name))
			return controller->streams[i];

	return NULL;
}

tfb_stream_t *tfb_controller_get_stream_by_id(tfb_controller_t *controller, int id) {
	for (int i=0; i<controller->num_streams; i++)
		if (id==controller->streams[i]->id)
			return controller->streams[i];

	return NULL;
}

bool tfb_controller_send_frame(tfb_controller_t *controller, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	return tfb_link_send(controller->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),flags);
}

int tfb_controller_get_available_stream_id(tfb_controller_t *controller) {
	int max_id=0;

	for (int i=0; i<controller->num_streams; i++)
		if (controller->streams[i]->id>max_id)
			max_id=controller->streams[i]->id;

	return max_id+1;
}

void tfb_controller_handle_frame(tfb_controller_t *controller, tfb_frame_t *frame) {
	if (tfb_frame_has_data(frame,TFB_ANNOUNCE_NAME)) {
		char *name=tfb_frame_get_strdup(frame,TFB_ANNOUNCE_NAME);
		tfb_stream_t *stream=tfb_controller_get_stream_by_name(controller,name);
		if (!stream) {
			int id=tfb_controller_get_available_stream_id(controller);
			//printf("creating stream id: %d\n",id);
			stream=tfb_stream_create_controlled(controller->link,id,name);
			controller->streams[controller->num_streams++]=stream;
			if (controller->stream_func)
				controller->stream_func(controller,stream);
		}

		//printf("sending assign...\n");
		tfb_frame_t *assignframe=tfb_frame_create(256);
		tfb_frame_write_data(assignframe,TFB_ASSIGN_NAME,(uint8_t*)name,strlen(name));
		tfb_frame_write_num(assignframe,TFB_TO,stream->id);
		tfb_frame_write_num(assignframe,TFB_SESSION_ID,controller->session_id);
		tfb_controller_send_frame(controller,assignframe,0);
		tfb_frame_dispose(assignframe);
		tfb_free(name);
	}

	if (tfb_frame_has_data(frame,TFB_FROM) &&
			!tfb_controller_get_stream_by_id(controller,tfb_frame_get_num(frame,TFB_FROM))) {
		tfb_frame_t *resetframe=tfb_frame_create(256);
		tfb_frame_write_num(resetframe,TFB_RESET_TO,tfb_frame_get_num(frame,TFB_FROM));
		tfb_frame_write_num(resetframe,TFB_SESSION_ID,controller->session_id);
		tfb_controller_send_frame(controller,resetframe,0);
		tfb_frame_dispose(resetframe);

		printf("unknown stream!!!\n");
	}
}

void tfb_controller_tick(tfb_controller_t *controller) {
	tfb_link_tick(controller->link);

	if (tfb_time_expired(controller->link->physical,controller->announcement_deadline)) {
		//printf("announcement expired!!!\n");
		tfb_frame_t *af=tfb_frame_create(32);
		tfb_frame_write_num(af,TFB_SESSION_ID,controller->session_id);
		tfb_controller_send_frame(controller,af,0);
		tfb_frame_dispose(af);
		controller->announcement_deadline=tfb_time_future(controller->link->physical,TFB_ANNOUNCEMENT_INTERVAL);
	}

	//for (int i=0; i<controller->num_streams; i++) {
	for (int i=controller->num_streams-1; i>=0; i--) {
		tfb_stream_tick(controller->streams[i]);
		if (!tfb_stream_is_connected(controller->streams[i])) {
			//printf("** removing stream\n");
			tfb_stream_t *stream=controller->streams[i];
			controller->num_streams--;
			memmove(&controller->streams[i],&controller->streams[i+1],(controller->num_streams-i)*sizeof(controller->streams[0]));
			tfb_stream_dispose(stream);
		}
	}

	//printf("controller peek size: %zu\n",tfb_link_peek_size(controller->link));

	if (tfb_link_peek_size(controller->link)) {
		size_t framesize=tfb_link_peek_size(controller->link);
		uint8_t *framedata=tfb_link_peek(controller->link);
		tfb_frame_t *frame=tfb_frame_create_from_data(framedata,framesize);
		tfb_controller_handle_frame(controller,frame);
		tfb_frame_dispose(frame);
		tfb_link_consume(controller->link);
	}
}

tfb_time_t tfb_controller_get_deadline(tfb_controller_t *controller) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(controller->link));
	deadline=tfb_time_soonest(deadline,controller->announcement_deadline);

	for (int i=0; i<controller->num_streams; i++)
		deadline=tfb_time_soonest(deadline,tfb_stream_get_deadline(controller->streams[i]));

	return deadline;
}

int tfb_controller_get_timeout(tfb_controller_t *controller) {
	return tfb_time_timeout(controller->link->physical,tfb_controller_get_deadline(controller));
}
