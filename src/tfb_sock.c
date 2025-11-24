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

bool tfb_sock_send_frame(tfb_sock_t *sock, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	return tfb_link_send(sock->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),flags);
}

void tfb_sock_announce(tfb_sock_t *sock) {
	tfb_frame_t *announceframe=tfb_frame_create(128);
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_NAME,(uint8_t *)sock->name,strlen(sock->name));
	tfb_sock_send_frame(sock,announceframe,0);
	tfb_frame_dispose(announceframe);
}

void tfb_sock_reset(tfb_sock_t *sock) {
	sock->id=0;
	sock->rx_pending=0;
	sock->rx_committed=0;
	sock->tx_pending=0;
	sock->tx_committed=0;
	sock->resend_level=0;
	sock->resend_deadline=TFB_TIME_NEVER;
	sock->activity_deadline=TFB_TIME_NEVER;
	sock->session_id=0;
}

tfb_sock_t *tfb_sock_create(tfb_physical_t *physical, char *name) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->event_func=NULL;
	sock->controlled_id=0;
	sock->name=tfb_strdup(name);
	sock->link=tfb_link_create(physical);
	sock->accepted=true;
	tfb_sock_reset(sock);
	tfb_sock_announce(sock);

	return sock;
}

tfb_sock_t *tfb_sock_create_controlled(tfb_link_t *link, char *name, int id) {
	tfb_sock_t *sock=tfb_malloc(sizeof(tfb_sock_t));

	sock->event_func=NULL;
	sock->name=tfb_strdup(name);
	sock->link=link;
	sock->accepted=false;
	tfb_sock_reset(sock);
	sock->id=id;
	sock->controlled_id=id;
	sock->activity_deadline=tfb_time_future(link->physical,TFB_CONNECTION_TIMEOUT);

	return sock;
}

void tfb_sock_dispose(tfb_sock_t *sock) {
	if (!sock->controlled_id)
		tfb_link_dispose(sock->link);

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

	uint32_t sofar=sock->rx_committed+sock->rx_pending;
	uint32_t inseq=tfb_frame_get_num(frame,TFB_SEQ);
	if (inseq>sofar)
		return;

	uint32_t offs=sofar-inseq;
	if (offs>tfb_frame_get_data_size(frame,TFB_PAYLOAD))
		return;

	uint8_t *data=tfb_frame_get_data(frame,TFB_PAYLOAD);
	uint32_t size=tfb_frame_get_data_size(frame,TFB_PAYLOAD)-offs;
	uint32_t available=TFB_BUFSIZE-sock->rx_pending;
	size=MIN(size,available);
	memcpy(&sock->rx_buf[sock->rx_pending],&data[offs],size);
	sock->rx_pending+=size;

	tfb_frame_t *ackframe=tfb_frame_create(64);
	tfb_sock_make_frame_ours(sock,ackframe);
	tfb_frame_write_num(ackframe,TFB_ACK,sock->rx_committed+sock->rx_pending);
	tfb_sock_send_frame(sock,ackframe,TFB_LINK_URGENT);
	tfb_frame_dispose(ackframe);

	if (tfb_sock_available(sock))
		tfb_sock_trigger_event(sock,TFB_EVENT_DATA);
}

void tfb_sock_handle_ack_frame(tfb_sock_t *sock, tfb_frame_t *frame) {
	bool filter=tfb_sock_is_frame_ours(sock,frame) &&
		tfb_frame_has_data(frame,TFB_ACK) &&
		tfb_frame_get_num(frame,TFB_ACK)>sock->tx_committed;

	if (!filter)
		return;

	int acked=tfb_frame_get_num(frame,TFB_ACK)-sock->tx_committed;
	if (acked<0) {
		printf("got ack for bytes not sent!!!\n");
		return;
	}

	if (acked>sock->tx_pending)
		acked=sock->tx_pending;

	//printf("pending: %d acked: %d\n",sock->tx_pending,acked);

	memmove(sock->tx_buf,&sock->tx_buf[acked],sock->tx_pending-acked);
	sock->tx_pending-=acked;
	sock->tx_committed+=acked;
	sock->resend_deadline=TFB_TIME_NEVER;
	sock->resend_level=0;

	if (sock->tx_pending) {
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
		tfb_sock_send_frame(sock,pongframe,0);
		tfb_frame_dispose(pongframe);
	}

	// Announce.
	if (!tfb_sock_is_connected(sock) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
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

	if (tfb_link_peek_size(sock->link)) {
		size_t framesize=tfb_link_peek_size(sock->link);
		uint8_t *framedata=tfb_link_peek(sock->link);
		tfb_frame_t *frame=tfb_frame_create_from_data(framedata,framesize);
		tfb_sock_handle_session_frame(sock,frame);
		tfb_sock_handle_controlled_frame(sock,frame);
		tfb_sock_handle_payload_frame(sock,frame);
		tfb_sock_handle_ack_frame(sock,frame);
		tfb_frame_dispose(frame);
		if (!sock->controlled_id)
			tfb_link_consume(sock->link);
	}

	if (tfb_time_expired(sock->link->physical,sock->resend_deadline)) {
		if (sock->resend_level>=TFB_RETRIES) {
			tfb_sock_reset(sock);
			tfb_sock_trigger_event(sock,TFB_EVENT_CLOSE);
		}

		else {
			size_t sendsize=MIN(sock->tx_pending,TFB_BUFSIZE-10);

			tfb_frame_t *frame=tfb_frame_create(sendsize+64);
			tfb_sock_make_frame_ours(sock,frame);
			tfb_frame_write_num(frame,TFB_SEQ,sock->tx_committed);
			tfb_frame_write_data(frame,TFB_PAYLOAD,sock->tx_buf,sendsize);
			tfb_sock_send_frame(sock,frame,0);
			tfb_frame_dispose(frame);

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

	size_t sendsize=MIN(size,TFB_BUFSIZE-sock->tx_pending);
	sendsize=MIN(sendsize,TFB_BUFSIZE-10);
	if (!sendsize)
		return 0;

	memcpy(&sock->tx_buf[sock->tx_pending],data,sendsize);
	tfb_frame_t *frame=tfb_frame_create(sendsize+64);
	tfb_sock_make_frame_ours(sock,frame);
	tfb_frame_write_num(frame,TFB_SEQ,sock->tx_committed+sock->tx_pending);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,sendsize);
	bool sendres=tfb_sock_send_frame(sock,frame,0);
	//printf("send res: %d\n",sendres);
	tfb_frame_dispose(frame);
	sock->tx_pending+=sendsize;

	if (sock->resend_deadline==TFB_TIME_NEVER) {
		sock->resend_level=0;
		sock->resend_deadline=tfb_time_future(sock->link->physical,TFB_RESEND_BASE<<sock->resend_level);
	}

	//printf("**** sent: %d, pending: %d\n",sendsize,sock->tx_pending);

	return sendsize;
}

size_t tfb_sock_available(tfb_sock_t *sock) {
	return sock->rx_pending;
}

tfb_time_t tfb_sock_get_deadline(tfb_sock_t *sock) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(sock->link));
	deadline=tfb_time_soonest(deadline,sock->activity_deadline);
	deadline=tfb_time_soonest(deadline,sock->resend_deadline);

	return deadline;
}

int tfb_sock_recv(tfb_sock_t *sock, uint8_t *data, size_t size) {
	size_t recvsize=MIN(size,tfb_sock_available(sock));
	memcpy(data,sock->rx_buf,recvsize);
	memmove(&sock->rx_buf[0],&sock->rx_buf[recvsize],sock->rx_pending-recvsize);
	sock->rx_pending-=recvsize;
	sock->rx_committed+=recvsize;

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
