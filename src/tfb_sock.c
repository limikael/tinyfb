#include "tfb_sock.h"
#include "tfb_physical.h"
#include "tfb_link.h"
#include "tfb_util.h"
#include "tfb_time.h"
#include "tfb_frame.h"
#include <string.h>
#include <stdio.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void tfb_sock_trigger_event(tfb_sock_t *sock, int event) {
	if (sock->event_func)
		sock->event_func(sock,event);
}

void tfb_sock_event_func(tfb_sock_t *sock, void (*event_func)(tfb_sock_t *sock, int event)) {
	sock->event_func=event_func;
}

void tfb_sock_announce(tfb_sock_t *sock) {
	tfb_frame_t *announceframe=tfb_frame_create(128);
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_NAME,(uint8_t *)sock->name,strlen(sock->name));
	tfb_link_send(sock->link,announceframe,TFB_LINK_SEND_OWNED);
}

void tfb_sock_reset(tfb_sock_t *sock) {
	sock->id=0;
	sock->resend_level=0;
	sock->resend_deadline=TFB_TIME_NEVER;
	sock->activity_deadline=TFB_TIME_NEVER;
	sock->session_id=0;
	sock->num_sent=0;
	sock->num_received=0;
	if (sock->data) {
		tfb_frame_dispose(sock->data);
		sock->data=NULL;
	}

	for (int i=0; i<sock->tx_queue_len; i++) {
		tfb_link_unsend(sock->link,sock->tx_queue[i]);
		tfb_frame_dispose(sock->tx_queue[i]);
	}

	sock->tx_queue_len=0;
}

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->tag=NULL;
	sock->event_func=NULL;
	sock->controlled_id=0;
	sock->name=tfb_strdup(name);
	sock->link=tfb_link_create(physical);
	sock->accepted=true;
	sock->tx_queue_len=0;
	sock->data=NULL;
	tfb_sock_reset(sock);
	tfb_sock_announce(sock);

	return sock;
}

tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, char *name, int id) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->tag=NULL;
	sock->event_func=NULL;
	sock->controlled_id=id;
	sock->name=tfb_strdup(name);
	sock->link=link;
	sock->accepted=false;
	sock->tx_queue_len=0;
	sock->data=NULL;
	tfb_sock_reset(sock);
	sock->id=id;
	sock->activity_deadline=tfb_time_future(link->physical,TFB_CONNECTION_TIMEOUT);

	return sock;
}

void tfb_sock_dispose(tfb_sock_t *sock) {
	if (!sock->controlled_id)
		tfb_link_dispose(sock->link);

	if (sock->data)
		tfb_frame_dispose(sock->data);

	for (int i=0; i<sock->tx_queue_len; i++) {
		tfb_link_unsend(sock->link,sock->tx_queue[i]);
		tfb_frame_dispose(sock->tx_queue[i]);
	}

	tfb_free(sock->name);
	tfb_free(sock);
}

bool tfb_sock_is_frame_ours(tfb_sock_t *sock, tfb_frame_t *frame) {
	if (!sock->controlled_id &&
			tfb_frame_has_data(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)==sock->id)
		return true;

	if (sock->controlled_id &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==sock->id)
		return true;

	return false;
}

void tfb_sock_make_frame_ours(tfb_sock_t *sock, tfb_frame_t *frame) {
	if (sock->controlled_id)
		tfb_frame_write_num(frame,TFB_TO,sock->id);

	else
		tfb_frame_write_num(frame,TFB_FROM,sock->id);
}

void tfb_sock_handle_payload_frame(tfb_sock_t *sock, tfb_frame_t *frame) {
	bool filter=tfb_sock_is_frame_ours(sock,frame) &&
		tfb_frame_has_data(frame,TFB_PAYLOAD) &&
		tfb_frame_has_data(frame,TFB_SEQ);

	if (!filter)
		return;

	if (tfb_frame_get_num(frame,TFB_SEQ)==sock->num_received &&
			!tfb_sock_available(sock)) {
		sock->data=tfb_frame_create_from_data(frame->buffer,frame->size);
		sock->num_received++;
	}

	if (tfb_frame_get_num(frame,TFB_SEQ)<sock->num_received) {
		tfb_frame_t *ackframe=tfb_frame_create(64);
		tfb_sock_make_frame_ours(sock,ackframe);
		tfb_frame_write_num(ackframe,TFB_ACK,tfb_frame_get_num(frame,TFB_SEQ));
		tfb_link_send(sock->link,ackframe,TFB_LINK_SEND_URGENT|TFB_LINK_SEND_OWNED);
	}

	if (tfb_sock_available(sock))
		tfb_sock_trigger_event(sock,TFB_EVENT_DATA);
}

uint8_t tfb_sock_get_num_acked(tfb_sock_t *sock) {
	return sock->num_sent-sock->tx_queue_len;
}

void tfb_sock_handle_ack_frame(tfb_sock_t *sock, tfb_frame_t *frame) {
	bool filter=tfb_sock_is_frame_ours(sock,frame) &&
		tfb_frame_has_data(frame,TFB_ACK) &&
		sock->tx_queue_len;

	if (!filter)
		return;

	int ack=tfb_frame_get_num(frame,TFB_ACK);
	int seq=tfb_frame_get_num(sock->tx_queue[0],TFB_SEQ);
	if (ack!=seq)
		return;

	tfb_link_unsend(sock->link,sock->tx_queue[0]);
	tfb_frame_dispose(sock->tx_queue[0]);
	sock->tx_queue_len--;
	memmove(&sock->tx_queue[0],&sock->tx_queue[1],sizeof(tfb_frame_t *)*sock->tx_queue_len);

	sock->resend_level=0;
	sock->resend_deadline=TFB_TIME_NEVER;

	if (sock->tx_queue_len) {
		tfb_link_send(sock->link,sock->tx_queue[0],0);
		sock->resend_level=0;
		sock->resend_deadline=tfb_time_future(sock->link->physical,TFB_RESEND_BASE<<sock->resend_level);
	}
}

void tfb_sock_handle_session_frame(tfb_sock_t *sock, tfb_frame_t *frame) {
	if (tfb_sock_is_controlled(sock))
		return;

	// Id reset.
	if (tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_RESET_TO) &&
			tfb_frame_get_num(frame,TFB_RESET_TO)==sock->id) {
		tfb_sock_reset(sock);
		tfb_sock_trigger_event(sock,TFB_EVENT_CLOSE);
	}

	// Session change, i.e. controller reset.
	if (tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			tfb_frame_get_num(frame,TFB_SESSION_ID)!=sock->session_id) {
		tfb_sock_reset(sock);
		tfb_sock_trigger_event(sock,TFB_EVENT_CLOSE);
	}

	// Assignment.
	if (!tfb_frame_strcmp(frame,TFB_ASSIGN_NAME,sock->name) &&
			tfb_frame_get_num(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)!=sock->id) {
		tfb_sock_reset(sock);
		sock->id=tfb_frame_get_num(frame,TFB_TO);
		sock->session_id=tfb_frame_get_num(frame,TFB_SESSION_ID);
		sock->activity_deadline=tfb_time_future(sock->link->physical,TFB_CONNECTION_TIMEOUT);
		tfb_sock_trigger_event(sock,TFB_EVENT_CONNECT);
	}

	// Pong.
	if (tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		sock->activity_deadline=tfb_time_future(sock->link->physical,TFB_CONNECTION_TIMEOUT);
		tfb_frame_t *pongframe=tfb_frame_create(32);
		tfb_frame_write_num(pongframe,TFB_FROM,sock->id);
		tfb_link_send(sock->link,pongframe,TFB_LINK_SEND_OWNED);
	}

	// Announce.
	if (!tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		//delay(100);
		tfb_sock_announce(sock);
	}
}

void tfb_sock_handle_controlled_frame(tfb_sock_t *sock, tfb_frame_t *frame) {
	if (!tfb_sock_is_controlled(sock))
		return;

	if (tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==sock->id) {
		sock->activity_deadline=tfb_time_future(sock->link->physical,TFB_CONNECTION_TIMEOUT);
	}
}

void tfb_sock_tick(tfb_sock_t *sock) {
	if (!tfb_sock_is_controlled(sock))
		tfb_link_tick(sock->link);

	if (tfb_link_peek(sock->link)) {
		//printf("have frame!!!\n");

		tfb_frame_t *frame=tfb_link_peek(sock->link);
		tfb_sock_handle_session_frame(sock,frame);
		tfb_sock_handle_controlled_frame(sock,frame);
		tfb_sock_handle_payload_frame(sock,frame);
		tfb_sock_handle_ack_frame(sock,frame);
		if (!sock->controlled_id)
			tfb_link_consume(sock->link);
	}

	if (tfb_time_expired(sock->link->physical,sock->resend_deadline)) {
		if (sock->resend_level>=TFB_RETRIES) {
			tfb_sock_reset(sock);
			tfb_sock_trigger_event(sock,TFB_EVENT_CLOSE);
		}

		else {
			tfb_link_send(sock->link,sock->tx_queue[0],0);
			sock->resend_level++;
			sock->resend_deadline=tfb_time_future(sock->link->physical,TFB_RESEND_BASE<<sock->resend_level);
		}
	}

	if (tfb_time_expired(sock->link->physical,sock->activity_deadline)) {
		tfb_sock_reset(sock);
		tfb_sock_trigger_event(sock,TFB_EVENT_CLOSE);
	}
}

int tfb_sock_send(tfb_sock_t *sock, uint8_t *data, size_t size) {
	if (!sock->id)
		return -1;

	if (sock->tx_queue_len>=TFB_SOCK_QUEUE_SIZE)
		return -1;

	tfb_frame_t *frame=tfb_frame_create(size+64);
	tfb_sock_make_frame_ours(sock,frame);
	tfb_frame_write_num(frame,TFB_SEQ,sock->num_sent++);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,size);
	sock->tx_queue[sock->tx_queue_len++]=frame;

	//printf("wrote, len=%zu, sent=%zu acked=%zu\n",sock->tx_queue_len,sock->num_sent,sock->num_acked);
	//printf("sent... queue len=%zu\n",sock->tx_queue_len);

	if (sock->tx_queue_len==1) {
		tfb_link_send(sock->link,sock->tx_queue[0],0);
		sock->resend_level=0;
		sock->resend_deadline=tfb_time_future(sock->link->physical,TFB_RESEND_BASE<<sock->resend_level);
	}

	return size;
}

tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(sock->link));
	deadline=tfb_time_soonest(deadline,sock->activity_deadline);
	deadline=tfb_time_soonest(deadline,sock->resend_deadline);

	return deadline;
}

int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size) {
	if (!sock->data)
		return 0;

	size_t recvsize=0;
	if (data) {
		recvsize=MIN(size,tfb_frame_get_data_size(sock->data,TFB_PAYLOAD));
		memcpy(data,tfb_frame_get_data(sock->data,TFB_PAYLOAD),recvsize);
	}

	tfb_frame_dispose(sock->data);
	sock->data=NULL;

	return recvsize;
}

bool tfb_sock_is_connected(tfb_sock_t *sock) {
	return (sock->id!=0);
}

bool tfb_sock_is_controlled(tfb_sock_t *sock) {
	return (sock->controlled_id!=0);
}

int tfb_sock_get_timeout(tfb_sock_t *sock) {
	return tfb_time_timeout(sock->link->physical,tfb_sock_get_deadline(sock));
}

size_t tfb_sock_available(tfb_sock_t *sock) {
	if (sock->data)
		return tfb_frame_get_data_size(sock->data,TFB_PAYLOAD);

	return 0;
}

bool tfb_sock_is_send_available(tfb_sock_t *sock) {
	return (sock->tx_queue_len<TFB_SOCK_QUEUE_SIZE);
}

bool tfb_sock_is_flushed(tfb_sock_t *sock) {
	return (sock->tx_queue_len==0);
}
