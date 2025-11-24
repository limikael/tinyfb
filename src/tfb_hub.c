#include "tfb_hub.h"
#include "tfb_link.h"
#include "tfb_util.h"
#include "tfb_frame.h"
#include "tfb_time.h"
#include "tfb_sock.h"
#include <stdio.h>
#include <string.h>

void tfb_hub_event_func(tfb_hub_t *hub, void (*event_func)(tfb_hub_t *hub, int event)) {
	hub->event_func=event_func;
}

tfb_hub_t *tfb_hub_create(tfb_physical_t *physical) {
	tfb_hub_t *hub=tfb_malloc(sizeof(tfb_hub_t));

	hub->link=tfb_link_create(physical);
	hub->num_socks=0;
	hub->session_id=(rand()%32000)+1;
	hub->announcement_deadline=tfb_time_now(physical);
	hub->event_func=NULL;

	return hub;
}

void tfb_hub_dispose(tfb_hub_t *hub) {
	for (int i=0; i<hub->num_socks; i++)
		tfb_sock_dispose(hub->socks[i]);

	tfb_link_dispose(hub->link);
	tfb_free(hub);
}

tfb_sock_t *tfb_hub_get_sock_by_name(tfb_hub_t *hub, char *name) {
	for (int i=0; i<hub->num_socks; i++)
		if (!strcmp(name,hub->socks[i]->name))
			return hub->socks[i];

	return NULL;
}

tfb_sock_t *tfb_hub_get_sock_by_id(tfb_hub_t *hub, int id) {
	for (int i=0; i<hub->num_socks; i++)
		if (id==hub->socks[i]->id)
			return hub->socks[i];

	return NULL;
}

bool tfb_hub_send_frame(tfb_hub_t *hub, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	return tfb_link_send(hub->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),flags);
}

int tfb_hub_get_available_sock_id(tfb_hub_t *hub) {
	int max_id=0;

	for (int i=0; i<hub->num_socks; i++)
		if (hub->socks[i]->id>max_id)
			max_id=hub->socks[i]->id;

	return max_id+1;
}

void tfb_hub_handle_frame(tfb_hub_t *hub, tfb_frame_t *frame) {
	if (tfb_frame_has_data(frame,TFB_ANNOUNCE_NAME)) {
		char *name=tfb_frame_get_strdup(frame,TFB_ANNOUNCE_NAME);
		tfb_sock_t *sock=tfb_hub_get_sock_by_name(hub,name);
		if (!sock) {
			int id=tfb_hub_get_available_sock_id(hub);
			//printf("creating sock id: %d\n",id);
			sock=tfb_sock_create_controlled(hub->link,name,id);
			hub->socks[hub->num_socks++]=sock;
			if (hub->event_func)
				hub->event_func(hub,TFB_EVENT_CONNECT);
		}

		//printf("sending assign...\n");
		tfb_frame_t *assignframe=tfb_frame_create(256);
		tfb_frame_write_data(assignframe,TFB_ASSIGN_NAME,(uint8_t*)name,strlen(name));
		tfb_frame_write_num(assignframe,TFB_TO,sock->id);
		tfb_frame_write_num(assignframe,TFB_SESSION_ID,hub->session_id);
		tfb_hub_send_frame(hub,assignframe,0);
		tfb_frame_dispose(assignframe);
		tfb_free(name);
	}

	if (tfb_frame_has_data(frame,TFB_FROM) &&
			!tfb_hub_get_sock_by_id(hub,tfb_frame_get_num(frame,TFB_FROM))) {
		tfb_frame_t *resetframe=tfb_frame_create(256);
		tfb_frame_write_num(resetframe,TFB_RESET_TO,tfb_frame_get_num(frame,TFB_FROM));
		tfb_frame_write_num(resetframe,TFB_SESSION_ID,hub->session_id);
		tfb_hub_send_frame(hub,resetframe,0);
		tfb_frame_dispose(resetframe);

		printf("unknown sock!!!\n");
	}
}

void tfb_hub_tick(tfb_hub_t *hub) {
	tfb_link_tick(hub->link);

	if (tfb_time_expired(hub->link->physical,hub->announcement_deadline)) {
		//printf("announcement expired!!!\n");
		tfb_frame_t *af=tfb_frame_create(32);
		tfb_frame_write_num(af,TFB_SESSION_ID,hub->session_id);
		tfb_hub_send_frame(hub,af,0);
		tfb_frame_dispose(af);
		hub->announcement_deadline=tfb_time_future(hub->link->physical,TFB_ANNOUNCEMENT_INTERVAL);
	}

	//for (int i=0; i<hub->num_socks; i++) {
	for (int i=hub->num_socks-1; i>=0; i--) {
		tfb_sock_tick(hub->socks[i]);
		if (!tfb_sock_is_connected(hub->socks[i])) {
			//printf("** removing sock\n");
			tfb_sock_t *sock=hub->socks[i];
			hub->num_socks--;
			memmove(&hub->socks[i],&hub->socks[i+1],(hub->num_socks-i)*sizeof(hub->socks[0]));
			tfb_sock_dispose(sock);
		}
	}

	//printf("hub peek size: %zu\n",tfb_link_peek_size(hub->link));

	if (tfb_link_peek_size(hub->link)) {
		size_t framesize=tfb_link_peek_size(hub->link);
		uint8_t *framedata=tfb_link_peek(hub->link);
		tfb_frame_t *frame=tfb_frame_create_from_data(framedata,framesize);
		tfb_hub_handle_frame(hub,frame);
		tfb_frame_dispose(frame);
		tfb_link_consume(hub->link);
	}
}

tfb_time_t tfb_hub_get_deadline(tfb_hub_t *hub) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(hub->link));
	deadline=tfb_time_soonest(deadline,hub->announcement_deadline);

	for (int i=0; i<hub->num_socks; i++)
		deadline=tfb_time_soonest(deadline,tfb_sock_get_deadline(hub->socks[i]));

	return deadline;
}

int tfb_hub_get_timeout(tfb_hub_t *hub) {
	return tfb_time_timeout(hub->link->physical,tfb_hub_get_deadline(hub));
}

tfb_sock_t *tfb_hub_accept(tfb_hub_t *hub) {
	for (int i=0; i<hub->num_socks; i++) {
		if (!hub->socks[i]->accepted) {
			hub->socks[i]->accepted=true;
			return hub->socks[i];
		}
	}

	return NULL;
}