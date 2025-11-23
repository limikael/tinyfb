#include "tfb_device.h"
#include "tfb_physical.h"
#include "tfb_link.h"
#include "tfb_util.h"
#include "tfb_time.h"
#include "tfb_frame.h"
#include <string.h>
#include <stdio.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

void tfb_device_data_func(tfb_device_t *device, void (*data_func)(tfb_device_t *device)) {
	device->data_func=data_func;
}

void tfb_device_status_func(tfb_device_t *device, void (*status_func)(tfb_device_t *device)) {
	device->status_func=status_func;
}

bool tfb_device_send_frame(tfb_device_t *device, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	return tfb_link_send(device->link,tfb_frame_get_buffer(frame),tfb_frame_get_size(frame),flags);
}

void tfb_device_announce(tfb_device_t *device) {
	tfb_frame_t *announceframe=tfb_frame_create(128);
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_NAME,(uint8_t *)device->name,strlen(device->name));
	tfb_device_send_frame(device,announceframe,0);
	tfb_frame_dispose(announceframe);
}

void tfb_device_reset(tfb_device_t *device) {
	device->id=0;
	device->rx_pending=0;
	device->rx_committed=0;
	device->tx_pending=0;
	device->tx_committed=0;
	device->resend_level=0;
	device->resend_deadline=TFB_TIME_NEVER;
	device->activity_deadline=TFB_TIME_NEVER;
	device->session_id=0;
}

tfb_device_t *tfb_device_create(tfb_physical_t *physical, char *name) {
	tfb_device_t *device=tfb_malloc(sizeof(tfb_device_t));

	device->data_func=NULL;
	device->status_func=NULL;
	device->controlled_id=0;
	device->name=tfb_strdup(name);
	device->link=tfb_link_create(physical);
	tfb_device_reset(device);
	tfb_device_announce(device);

	return device;
}

tfb_device_t *tfb_device_create_controlled(tfb_link_t *link, int id, char *name) {
	tfb_device_t *device=tfb_malloc(sizeof(tfb_device_t));

	device->data_func=NULL;
	device->status_func=NULL;
	device->name=tfb_strdup(name);
	device->link=link;
	tfb_device_reset(device);
	device->id=id;
	device->controlled_id=id;
	device->activity_deadline=tfb_time_future(link->physical,TFB_CONNECTION_TIMEOUT);

	return device;
}

void tfb_device_dispose(tfb_device_t *device) {
	if (!device->controlled_id)
		tfb_link_dispose(device->link);

	tfb_free(device->name);
	tfb_free(device);
}

bool tfb_device_is_frame_ours(tfb_device_t *device, tfb_frame_t *frame) {
	if (!device->controlled_id &&
			tfb_frame_has_data(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)==device->id)
		return true;

	if (device->controlled_id &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==device->id)
		return true;

	return false;
}

void tfb_device_make_frame_ours(tfb_device_t *device, tfb_frame_t *frame) {
	if (device->controlled_id)
		tfb_frame_write_num(frame,TFB_TO,device->id);

	else
		tfb_frame_write_num(frame,TFB_FROM,device->id);
}

void tfb_device_handle_payload_frame(tfb_device_t *device, tfb_frame_t *frame) {
	bool filter=tfb_device_is_frame_ours(device,frame) &&
		tfb_frame_has_data(frame,TFB_PAYLOAD) &&
		tfb_frame_has_data(frame,TFB_SEQ);

	if (!filter)
		return;

	uint32_t sofar=device->rx_committed+device->rx_pending;
	uint32_t inseq=tfb_frame_get_num(frame,TFB_SEQ);
	if (inseq>sofar)
		return;

	uint32_t offs=sofar-inseq;
	if (offs>tfb_frame_get_data_size(frame,TFB_PAYLOAD))
		return;

	uint8_t *data=tfb_frame_get_data(frame,TFB_PAYLOAD);
	uint32_t size=tfb_frame_get_data_size(frame,TFB_PAYLOAD)-offs;
	uint32_t available=TFB_TRANSPORT_BUF_SIZE-device->rx_pending;
	size=MIN(size,available);
	memcpy(&device->rx_buf[device->rx_pending],&data[offs],size);
	device->rx_pending+=size;

	tfb_frame_t *ackframe=tfb_frame_create(64);
	tfb_device_make_frame_ours(device,ackframe);
	tfb_frame_write_num(ackframe,TFB_ACK,device->rx_committed+device->rx_pending);
	tfb_device_send_frame(device,ackframe,TFB_LINK_URGENT);
	tfb_frame_dispose(ackframe);

	if (device->data_func && tfb_device_available(device))
		device->data_func(device);
}

void tfb_device_handle_ack_frame(tfb_device_t *device, tfb_frame_t *frame) {
	bool filter=tfb_device_is_frame_ours(device,frame) &&
		tfb_frame_has_data(frame,TFB_ACK) &&
		tfb_frame_get_num(frame,TFB_ACK)>device->tx_committed;

	if (!filter)
		return;

	int acked=tfb_frame_get_num(frame,TFB_ACK)-device->tx_committed;
	if (acked<0) {
		printf("got ack for bytes not sent!!!\n");
		return;
	}

	if (acked>device->tx_pending)
		acked=device->tx_pending;

	//printf("pending: %d acked: %d\n",device->tx_pending,acked);

	memmove(device->tx_buf,&device->tx_buf[acked],device->tx_pending-acked);
	device->tx_pending-=acked;
	device->tx_committed+=acked;
	device->resend_deadline=TFB_TIME_NEVER;
	device->resend_level=0;

	if (device->tx_pending) {
		device->resend_level=0;
		device->resend_deadline=tfb_time_future(device->link->physical,TFB_RESEND_BASE<<device->resend_level);
	}
}

void tfb_device_handle_session_frame(tfb_device_t *device, tfb_frame_t *frame) {
	if (tfb_device_is_controlled(device))
		return;

	// Id reset.
	if (tfb_device_is_connected(device) &&
			tfb_frame_has_data(frame,TFB_RESET_TO) &&
			tfb_frame_get_num(frame,TFB_RESET_TO)==device->id) {
		tfb_device_reset(device);
		if (device->status_func)
			device->status_func(device);
	}

	// Session change, i.e. controller reset.
	if (tfb_device_is_connected(device) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			tfb_frame_get_num(frame,TFB_SESSION_ID)!=device->session_id) {
		tfb_device_reset(device);
		if (device->status_func)
			device->status_func(device);
	}

	// Assignment.
	if (!tfb_frame_strcmp(frame,TFB_ASSIGN_NAME,device->name) &&
			tfb_frame_get_num(frame,TFB_TO) &&
			tfb_frame_get_num(frame,TFB_TO)!=device->id) {
		tfb_device_reset(device);
		device->id=tfb_frame_get_num(frame,TFB_TO);
		device->session_id=tfb_frame_get_num(frame,TFB_SESSION_ID);
		device->activity_deadline=tfb_time_future(device->link->physical,TFB_CONNECTION_TIMEOUT);
		if (device->status_func)
			device->status_func(device);
	}

	// Pong.
	if (tfb_device_is_connected(device) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		device->activity_deadline=tfb_time_future(device->link->physical,TFB_CONNECTION_TIMEOUT);
		tfb_frame_t *pongframe=tfb_frame_create(32);
		tfb_frame_write_num(pongframe,TFB_FROM,device->id);
		tfb_device_send_frame(device,pongframe,0);
		tfb_frame_dispose(pongframe);
	}

	// Announce.
	if (!tfb_device_is_connected(device) &&
			tfb_frame_has_data(frame,TFB_SESSION_ID) &&
			!tfb_frame_has_data(frame,TFB_ASSIGN_NAME)) {
		tfb_device_announce(device);
	}
}

void tfb_device_handle_controlled_frame(tfb_device_t *device, tfb_frame_t *frame) {
	if (!tfb_device_is_controlled(device))
		return;

	if (tfb_device_is_connected(device) &&
			tfb_frame_has_data(frame,TFB_FROM) &&
			tfb_frame_get_num(frame,TFB_FROM)==device->id) {
		device->activity_deadline=tfb_time_future(device->link->physical,TFB_CONNECTION_TIMEOUT);
	}
}

void tfb_device_tick(tfb_device_t *device) {
	if (!tfb_device_is_controlled(device))
		tfb_link_tick(device->link);

	if (tfb_link_peek_size(device->link)) {
		size_t framesize=tfb_link_peek_size(device->link);
		uint8_t *framedata=tfb_link_peek(device->link);
		tfb_frame_t *frame=tfb_frame_create_from_data(framedata,framesize);
		tfb_device_handle_session_frame(device,frame);
		tfb_device_handle_controlled_frame(device,frame);
		tfb_device_handle_payload_frame(device,frame);
		tfb_device_handle_ack_frame(device,frame);
		tfb_frame_dispose(frame);
		if (!device->controlled_id)
			tfb_link_consume(device->link);
	}

	if (tfb_time_expired(device->link->physical,device->resend_deadline)) {
		if (device->resend_level>=TFB_RETRIES) {
			tfb_device_reset(device);
			if (device->status_func)
				device->status_func(device);
		}

		else {
			size_t sendsize=MIN(device->tx_pending,TFB_LINK_TX_BUF_SIZE-10);

			tfb_frame_t *frame=tfb_frame_create(sendsize+64);
			tfb_device_make_frame_ours(device,frame);
			tfb_frame_write_num(frame,TFB_SEQ,device->tx_committed);
			tfb_frame_write_data(frame,TFB_PAYLOAD,device->tx_buf,sendsize);
			tfb_device_send_frame(device,frame,0);
			tfb_frame_dispose(frame);

			device->resend_level++;
			device->resend_deadline=tfb_time_future(device->link->physical,TFB_RESEND_BASE<<device->resend_level);
		}
	}

	if (tfb_time_expired(device->link->physical,device->activity_deadline)) {
		tfb_device_reset(device);
		if (device->status_func)
			device->status_func(device);
	}
}

int tfb_device_send(tfb_device_t *device, uint8_t *data, size_t size) {
	if (!device->id)
		return -1;

	size_t sendsize=MIN(size,TFB_TRANSPORT_BUF_SIZE-device->tx_pending);
	sendsize=MIN(sendsize,TFB_LINK_TX_BUF_SIZE-10);
	if (!sendsize)
		return 0;

	memcpy(&device->tx_buf[device->tx_pending],data,sendsize);
	tfb_frame_t *frame=tfb_frame_create(sendsize+64);
	tfb_device_make_frame_ours(device,frame);
	tfb_frame_write_num(frame,TFB_SEQ,device->tx_committed+device->tx_pending);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,sendsize);
	bool sendres=tfb_device_send_frame(device,frame,0);
	//printf("send res: %d\n",sendres);
	tfb_frame_dispose(frame);
	device->tx_pending+=sendsize;

	if (device->resend_deadline==TFB_TIME_NEVER) {
		device->resend_level=0;
		device->resend_deadline=tfb_time_future(device->link->physical,TFB_RESEND_BASE<<device->resend_level);
	}

	return sendsize;
}

size_t tfb_device_available(tfb_device_t *device) {
	return device->rx_pending;
}

int tfb_device_read_byte(tfb_device_t *device) {
	if (!tfb_device_available(device))
		return -1;

	uint8_t byte;
	tfb_device_recv(device,&byte,1);

	return byte;
}

tfb_time_t tfb_device_get_deadline(tfb_device_t *device) {
	tfb_time_t deadline=TFB_TIME_NEVER;

	deadline=tfb_time_soonest(deadline,tfb_link_get_deadline(device->link));
	deadline=tfb_time_soonest(deadline,device->activity_deadline);
	deadline=tfb_time_soonest(deadline,device->resend_deadline);

	return deadline;
}

int tfb_device_get_timeout(tfb_device_t *device) {
	return tfb_time_timeout(device->link->physical,tfb_device_get_deadline(device));
}

int tfb_device_recv(tfb_device_t *device, uint8_t *data, size_t size) {
	size_t recvsize=MIN(size,tfb_device_available(device));
	memcpy(data,device->rx_buf,recvsize);
	memmove(&device->rx_buf[0],&device->rx_buf[recvsize],device->rx_pending-recvsize);
	device->rx_pending-=recvsize;
	device->rx_committed+=recvsize;

	return recvsize;
}

bool tfb_device_is_connected(tfb_device_t *device) {
	return (device->id!=0);
}

bool tfb_device_is_controlled(tfb_device_t *device) {
	return (device->controlled_id!=0);
}
