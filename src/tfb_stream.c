#include "tfb_stream.h"
#include "tfb_physical.h"
#include "tfb_link.h"
#include "tfb_util.h"
#include "tfb_time.h"
#include "tfb_frame.h"
#include <string.h>
#include <stdio.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void tfb_stream_trigger_event(tfb_stream_t *stream, int event) {
	if (stream->event_func)
		stream->event_func(stream,event);
}

void tfb_stream_event_func(tfb_stream_t *stream, void (*event_func)(tfb_stream_t *stream, int event)) {
	stream->event_func=event_func;
}

bool tfb_stream_send_frame(tfb_stream_t *stream, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	return tfb_link_send(stream->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),flags);
}

void tfb_stream_announce(tfb_stream_t *stream) {
	tfb_frame_t *announceframe=tfb_frame_create(128);
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_NAME,(uint8_t *)stream->name,strlen(stream->name));
	tfb_stream_send_frame(stream,announceframe,0);
	tfb_frame_dispose(announceframe);
}

void tfb_stream_reset(tfb_stream_t *stream) {
	stream->id=0;
	stream->rx_pending=0;
	stream->rx_committed=0;
	stream->tx_pending=0;
	stream->tx_committed=0;
	stream->resend_level=0;
	stream->resend_deadline=TFB_TIME_NEVER;
	stream->activity_deadline=TFB_TIME_NEVER;
	stream->session_id=0;
}

tfb_stream_t *tfb_stream_create(tfb_physical_t *physical, char *name) {
	tfb_stream_t *stream=tfb_malloc(sizeof(tfb_stream_t));

	stream->event_func=NULL;
	stream->controlled_id=0;
	stream->name=tfb_strdup(name);
	stream->link=tfb_link_create(physical);
	tfb_stream_reset(stream);
	tfb_stream_announce(stream);

	return stream;
}

tfb_stream_t *tfb_stream_create_controlled(tfb_link_t *link, char *name, int id) {
	tfb_stream_t *stream=tfb_malloc(sizeof(tfb_stream_t));

	stream->event_func=NULL;
	stream->name=tfb_strdup(name);
	stream->link=link;
	tfb_stream_reset(stream);
	stream->id=id;
	stream->controlled_id=id;
	stream->activity_deadline=tfb_time_future(link->physical,TFB_CONNECTION_TIMEOUT);

	return stream;
}

void tfb_stream_dispose(tfb_stream_t *stream) {
	if (!stream->controlled_id)
		tfb_link_dispose(stream->link);

	tfb_free(stream->name);
	tfb_free(stream);
}

bool tfb_stream_is_frame_ours(tfb_stream_t *stream, tfb_frame_t *frame) {
	if (!stream->controlled_id &&
			tfb_frame_has_data(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)==stream->id)
		return true;

	if (stream->controlled_id &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==stream->id)
		return true;

	return false;
}

void tfb_stream_make_frame_ours(tfb_stream_t *stream, tfb_frame_t *frame) {
	if (stream->controlled_id)
		tfb_frame_write_num(frame,TFB_TO,stream->id);

	else
		tfb_frame_write_num(frame,TFB_FROM,stream->id);
}

void tfb_stream_handle_payload_frame(tfb_stream_t *stream, tfb_frame_t *frame) {
	bool filter=tfb_stream_is_frame_ours(stream,frame) &&
		tfb_frame_has_data(frame,TFB_PAYLOAD) &&
		tfb_frame_has_data(frame,TFB_SEQ);

	if (!filter)
		return;

	uint32_t sofar=stream->rx_committed+stream->rx_pending;
	uint32_t inseq=tfb_frame_get_num(frame,TFB_SEQ);
	if (inseq>sofar)
		return;

	uint32_t offs=sofar-inseq;
	if (offs>tfb_frame_get_data_size(frame,TFB_PAYLOAD))
		return;

	uint8_t *data=tfb_frame_get_data(frame,TFB_PAYLOAD);
	uint32_t size=tfb_frame_get_data_size(frame,TFB_PAYLOAD)-offs;
	uint32_t available=TFB_TRANSPORT_BUF_SIZE-stream->rx_pending;
	size=MIN(size,available);
	memcpy(&stream->rx_buf[stream->rx_pending],&data[offs],size);
	stream->rx_pending+=size;

	tfb_frame_t *ackframe=tfb_frame_create(64);
	tfb_stream_make_frame_ours(stream,ackframe);
	tfb_frame_write_num(ackframe,TFB_ACK,stream->rx_committed+stream->rx_pending);
	tfb_stream_send_frame(stream,ackframe,TFB_LINK_URGENT);
	tfb_frame_dispose(ackframe);

	if (tfb_stream_available(stream))
		tfb_stream_trigger_event(stream,TFB_EVENT_DATA);
}

void tfb_stream_handle_ack_frame(tfb_stream_t *stream, tfb_frame_t *frame) {
	bool filter=tfb_stream_is_frame_ours(stream,frame) &&
		tfb_frame_has_data(frame,TFB_ACK) &&
		tfb_frame_get_num(frame,TFB_ACK)>stream->tx_committed;

	if (!filter)
		return;

	int acked=tfb_frame_get_num(frame,TFB_ACK)-stream->tx_committed;
	if (acked<0) {
		printf("got ack for bytes not sent!!!\n");
		return;
	}

	if (acked>stream->tx_pending)
		acked=stream->tx_pending;

	//printf("pending: %d acked: %d\n",stream->tx_pending,acked);

	memmove(stream->tx_buf,&stream->tx_buf[acked],stream->tx_pending-acked);
	stream->tx_pending-=acked;
	stream->tx_committed+=acked;
	stream->resend_deadline=TFB_TIME_NEVER;
	stream->resend_level=0;

	if (stream->tx_pending) {
		stream->resend_level=0;
		stream->resend_deadline=tfb_time_future(stream->link->physical,TFB_RESEND_BASE<<stream->resend_level);
	}
}

void tfb_stream_handle_session_frame(tfb_stream_t *stream, tfb_frame_t *frame) {
	if (tfb_stream_is_controlled(stream))
		return;

	// Id reset.
	if (tfb_stream_is_connected(stream) &&
			tfb_frame_has_data(frame,TFB_RESET_TO) &&
			tfb_frame_get_num(frame,TFB_RESET_TO)==stream->id) {
		tfb_stream_reset(stream);
		tfb_stream_trigger_event(stream,TFB_EVENT_CLOSE);
	}

	// Session change, i.e. controller reset.
	if (tfb_stream_is_connected(stream) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			tfb_frame_get_num(frame,TFB_SESSION_ID)!=stream->session_id) {
		tfb_stream_reset(stream);
		tfb_stream_trigger_event(stream,TFB_EVENT_CLOSE);
	}

	// Assignment.
	if (!tfb_frame_strcmp(frame,TFB_ASSIGN_NAME,stream->name) &&
			tfb_frame_get_num(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)!=stream->id) {
		tfb_stream_reset(stream);
		stream->id=tfb_frame_get_num(frame,TFB_TO);
		stream->session_id=tfb_frame_get_num(frame,TFB_SESSION_ID);
		stream->activity_deadline=tfb_time_future(stream->link->physical,TFB_CONNECTION_TIMEOUT);
		tfb_stream_trigger_event(stream,TFB_EVENT_CONNECT);
	}

	// Pong.
	if (tfb_stream_is_connected(stream) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		stream->activity_deadline=tfb_time_future(stream->link->physical,TFB_CONNECTION_TIMEOUT);
		tfb_frame_t *pongframe=tfb_frame_create(32);
		tfb_frame_write_num(pongframe,TFB_FROM,stream->id);
		tfb_stream_send_frame(stream,pongframe,0);
		tfb_frame_dispose(pongframe);
	}

	// Announce.
	if (!tfb_stream_is_connected(stream) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		tfb_stream_announce(stream);
	}
}

void tfb_stream_handle_controlled_frame(tfb_stream_t *stream, tfb_frame_t *frame) {
	if (!tfb_stream_is_controlled(stream))
		return;

	if (tfb_stream_is_connected(stream) &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==stream->id) {
		stream->activity_deadline=tfb_time_future(stream->link->physical,TFB_CONNECTION_TIMEOUT);
	}
}

void tfb_stream_tick(tfb_stream_t *stream) {
	if (!tfb_stream_is_controlled(stream))
		tfb_link_tick(stream->link);

	if (tfb_link_peek_size(stream->link)) {
		size_t framesize=tfb_link_peek_size(stream->link);
		uint8_t *framedata=tfb_link_peek(stream->link);
		tfb_frame_t *frame=tfb_frame_create_from_data(framedata,framesize);
		tfb_stream_handle_session_frame(stream,frame);
		tfb_stream_handle_controlled_frame(stream,frame);
		tfb_stream_handle_payload_frame(stream,frame);
		tfb_stream_handle_ack_frame(stream,frame);
		tfb_frame_dispose(frame);
		if (!stream->controlled_id)
			tfb_link_consume(stream->link);
	}

	if (tfb_time_expired(stream->link->physical,stream->resend_deadline)) {
		if (stream->resend_level>=TFB_RETRIES) {
			tfb_stream_reset(stream);
			tfb_stream_trigger_event(stream,TFB_EVENT_CLOSE);
		}

		else {
			size_t sendsize=MIN(stream->tx_pending,TFB_LINK_TX_BUF_SIZE-10);

			tfb_frame_t *frame=tfb_frame_create(sendsize+64);
			tfb_stream_make_frame_ours(stream,frame);
			tfb_frame_write_num(frame,TFB_SEQ,stream->tx_committed);
			tfb_frame_write_data(frame,TFB_PAYLOAD,stream->tx_buf,sendsize);
			tfb_stream_send_frame(stream,frame,0);
			tfb_frame_dispose(frame);

			stream->resend_level++;
			stream->resend_deadline=tfb_time_future(stream->link->physical,TFB_RESEND_BASE<<stream->resend_level);
		}
	}

	if (tfb_time_expired(stream->link->physical,stream->activity_deadline)) {
		tfb_stream_reset(stream);
		tfb_stream_trigger_event(stream,TFB_EVENT_CLOSE);
	}
}

int tfb_stream_send(tfb_stream_t *stream, uint8_t *data, size_t size) {
	if (!stream->id)
		return -1;

	size_t sendsize=MIN(size,TFB_TRANSPORT_BUF_SIZE-stream->tx_pending);
	sendsize=MIN(sendsize,TFB_LINK_TX_BUF_SIZE-10);
	if (!sendsize)
		return 0;

	memcpy(&stream->tx_buf[stream->tx_pending],data,sendsize);
	tfb_frame_t *frame=tfb_frame_create(sendsize+64);
	tfb_stream_make_frame_ours(stream,frame);
	tfb_frame_write_num(frame,TFB_SEQ,stream->tx_committed+stream->tx_pending);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,sendsize);
	bool sendres=tfb_stream_send_frame(stream,frame,0);
	//printf("send res: %d\n",sendres);
	tfb_frame_dispose(frame);
	stream->tx_pending+=sendsize;

	if (stream->resend_deadline==TFB_TIME_NEVER) {
		stream->resend_level=0;
		stream->resend_deadline=tfb_time_future(stream->link->physical,TFB_RESEND_BASE<<stream->resend_level);
	}

	return sendsize;
}

size_t tfb_stream_available(tfb_stream_t *stream) {
	return stream->rx_pending;
}

/*int tfb_stream_read_byte(tfb_stream_t *stream) {
	if (!tfb_stream_available(stream))
		return -1;

	uint8_t byte;
	tfb_stream_recv(stream,&byte,1);

	return byte;
}*/

tfb_time_t tfb_stream_get_deadline(tfb_stream_t *stream) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(stream->link));
	deadline=tfb_time_soonest(deadline,stream->activity_deadline);
	deadline=tfb_time_soonest(deadline,stream->resend_deadline);

	return deadline;
}

/*int tfb_stream_get_timeout(tfb_stream_t *stream) {
	return tfb_time_timeout(stream->link->physical,tfb_stream_get_deadline(stream));
}*/

int tfb_stream_recv(tfb_stream_t *stream, uint8_t *data, size_t size) {
	size_t recvsize=MIN(size,tfb_stream_available(stream));
	memcpy(data,stream->rx_buf,recvsize);
	memmove(&stream->rx_buf[0],&stream->rx_buf[recvsize],stream->rx_pending-recvsize);
	stream->rx_pending-=recvsize;
	stream->rx_committed+=recvsize;

	return recvsize;
}

bool tfb_stream_is_connected(tfb_stream_t *stream) {
	return (stream->id!=0);
}

bool tfb_stream_is_controlled(tfb_stream_t *stream) {
	return (stream->controlled_id!=0);
}
