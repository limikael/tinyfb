#include "tfb.h"
#include "tfb_frame.h"
#include "tfb_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

uint32_t (*tfb_millis)()=NULL;

void tfb_dispose_frame(tfb_t *tfb, tfb_frame_t *frame) {
	//printf("dispose frame called!!!\n");

	for (int i=0; i<tfb->tx_queue_len; i++) {
		if (tfb->tx_queue[i]==frame) {
			pointer_array_remove((void**)tfb->tx_queue,&tfb->tx_queue_len,i);
			i--;
		}
	}

	if (tfb->tx_frame==frame)
		tfb->tx_frame=NULL;

	tfb_frame_dispose(frame);
}

int tfb_get_device_id_by_name(tfb_t *tfb, char *name) {
	for (int i=0; i<tfb->num_devices; i++)
		if (!strcmp(name,tfb->devices[i]->name))
			return tfb->devices[i]->id;

	return 0;
}

int tfb_device_id_by_name(tfb_t *tfb, char *s) {
	if (!tfb_is_controller(tfb))
		return 0;

	for (int i=0; i<tfb->num_devices; i++)
		if (!strcmp(tfb->devices[i]->name,s))
			return tfb->devices[i]->id;

	return 0;
}

void tfb_schedule_resend(tfb_t *tfb, tfb_frame_t *frame) {
	frame->sent_times++;
	frame->send_at=tfb_millis()+(TFB_RESEND_BASE<<frame->sent_times);
}

void tfb_schedule_announcement(tfb_t *tfb, tfb_frame_t *frame) {
	tfb_dispose_frame(tfb,frame);
	tfb->announcement_deadline=tfb_millis()+TFB_ANNOUNCEMENT_INTERVAL;
}

tfb_t *tfb_create() {
	if (!tfb_millis) {
		printf("**** millis function not set! ****\n");
		return NULL;
	}

	//printf("millis=%p\n",tfb_millis);

	tfb_t *tfb=tfb_malloc(sizeof(tfb_t));
	tfb->rx_frame=tfb_frame_create(TFB_RX_BUF_SIZE);
	tfb->tx_frame=NULL;
	tfb->id=-1;
	tfb->message_func=NULL;
	tfb->message_from_func=NULL;
	tfb->tx_queue_len=0;
	tfb->bus_available_millis=0;
	tfb->seq=1;
	tfb->device_name=NULL;
	tfb->device_type=NULL;
	tfb->announcement_deadline=0;
	tfb->num_devices=0;
	tfb->devices=NULL;
	tfb->session_id=0;
	tfb->device_func=NULL;

	tfb_notify_bus_activity(tfb);

	return tfb;
}

tfb_frame_t *tfb_create_announce_session_frame(tfb_t *tfb) {
	tfb_frame_t *sessionframe=tfb_frame_create(1024);
	tfb_frame_set_notification_func(sessionframe,tfb,tfb_schedule_announcement);
	tfb_frame_write_num(sessionframe,TFB_SESSION_ID,tfb->session_id);
	tfb_frame_write_checksum(sessionframe);

	return sessionframe;
}

tfb_frame_t *tfb_create_announce_device_frame(tfb_t *tfb) {
	tfb_frame_t *announceframe=tfb_frame_create(1024);
	tfb_frame_set_notification_func(announceframe,tfb,tfb_dispose_frame);
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_NAME,(uint8_t *)tfb->device_name,strlen(tfb->device_name));
	tfb_frame_write_data(announceframe,TFB_ANNOUNCE_TYPE,(uint8_t *)tfb->device_type,strlen(tfb->device_type));
	tfb_frame_write_checksum(announceframe);

	return announceframe;
}

tfb_t *tfb_create_controller() {
	tfb_t *tfb=tfb_create();
	tfb->id=0;
	tfb->devices=tfb_malloc(sizeof(tfb_device_t *)*16); // TODO: make dynamic
	tfb->session_id=(rand()%32000)+1;
	tfb->tx_queue[tfb->tx_queue_len++]=tfb_create_announce_session_frame(tfb);

	return tfb;
}

tfb_t *tfb_create_device(char *name, char *type) {
	tfb_t *tfb=tfb_create();
	tfb->device_name=tfb_strdup(name);
	tfb->device_type=tfb_strdup(type);
	tfb->id=-1;

	//printf("will send device announcement, name=%s, type=%s\n",tfb->device_name,tfb->device_type);
	tfb->tx_queue[tfb->tx_queue_len++]=tfb_create_announce_device_frame(tfb);

	return tfb;
}

bool tfb_is_device(tfb_t *tfb) {
	return (tfb->id!=0);
}

bool tfb_is_controller(tfb_t *tfb) {
	return (tfb->id==0);
}

bool tfb_is_connected(tfb_t *tfb) {
	if (tfb_is_controller(tfb))
		return true;

	return (tfb->id>0);
}

void tfb_message_func(tfb_t *tfb, void (*func)(uint8_t *data, size_t size)) {
	tfb->message_func=func;
}

void tfb_message_from_func(tfb_t *tfb, void (*func)(uint8_t *data, size_t size, int from)) {
	tfb->message_from_func=func;
}

void tfb_device_func(tfb_t *tfb, void (*func)(char *name)) {
	tfb->device_func=func;
}

void tfb_dispose(tfb_t *tfb) {
	tfb_frame_dispose(tfb->rx_frame);
	if (tfb->tx_frame)
		tfb_dispose_frame(tfb,tfb->tx_frame);

	while (tfb->tx_queue_len)
		tfb_dispose_frame(tfb,tfb->tx_queue[0]);

	if (tfb->device_name)
		tfb_free(tfb->device_name);

	if (tfb->device_type)
		tfb_free(tfb->device_type);

	for (int i=0; i<tfb->num_devices; i++)
		tfb_device_dispose(tfb->devices[i]);

	if (tfb->devices)
		tfb_free(tfb->devices);

	tfb_free(tfb);
}

void tfb_set_id(tfb_t *tfb, int id) {
	if (tfb->tx_frame)
		tfb_dispose_frame(tfb,tfb->tx_frame);

	while (tfb->tx_queue_len)
		tfb_dispose_frame(tfb,tfb->tx_queue[0]);

	tfb->id=id;
}

void tfb_tx_make_current(tfb_t *tfb, tfb_frame_t *frame) {
	tfb_frame_tx_rewind(frame);
	tfb->tx_frame=frame;
}

bool tfb_is_frame_message_for_us(tfb_t *tfb, tfb_frame_t *frame) {
	if (tfb_is_device(tfb) && tfb_is_connected(tfb)) {
		if (!tfb_frame_has_data(frame,TFB_TO))
			return false;

		if (tfb_frame_get_num(frame,TFB_TO)==tfb->id)
			return true;
	}

	if (tfb_is_controller(tfb)) {
		if (!tfb_frame_has_data(frame,TFB_TO) &&
				tfb_frame_has_data(frame,TFB_FROM))
			return true;
	}

	return false;
}

int tfb_get_available_device_id(tfb_t *tfb) {
	int max_id=0;

	for (int i=0; i<tfb->num_devices; i++)
		if (tfb->devices[i]->id>max_id)
			max_id=tfb->devices[i]->id;

	return max_id+1;
}

void tfb_rx_push_byte(tfb_t *tfb, uint8_t byte) {
	if (tfb->tx_frame)
		return;

	tfb_notify_bus_activity(tfb);
	tfb_frame_rx_push_byte(tfb->rx_frame,byte);

	if (!tfb_frame_rx_is_complete(tfb->rx_frame))
		return;

	if (tfb_frame_get_checksum(tfb->rx_frame)) {
		tfb_frame_reset(tfb->rx_frame);
		return;
	}

	if (tfb_is_frame_message_for_us(tfb,tfb->rx_frame)) {
		//printf("processing message frame!!!\n");

		if (tfb_frame_has_data(tfb->rx_frame,TFB_PAYLOAD)) {
			uint8_t *payload=tfb_frame_get_data(tfb->rx_frame,TFB_PAYLOAD);
			size_t payload_size=tfb_frame_get_data_size(tfb->rx_frame,TFB_PAYLOAD);

			tfb_frame_t *ackframe=tfb_frame_create(128);
			tfb_frame_set_notification_func(ackframe,tfb,tfb_dispose_frame);
			//tfb_frame_set_auto_dispose(ackframe,true);

			if (tfb_is_controller(tfb)) {
				//printf("acking in controller to: %d\n",tfb_frame_get_num(tfb->rx_frame,TFB_FROM));
				tfb_frame_write_num(ackframe,TFB_TO,tfb_frame_get_num(tfb->rx_frame,TFB_FROM));
			}

			if (tfb_is_device(tfb))
				tfb_frame_write_num(ackframe,TFB_FROM,tfb->id);

			tfb_frame_write_num(ackframe,TFB_ACK,tfb_frame_get_num(tfb->rx_frame,TFB_SEQ));
			tfb_frame_write_checksum(ackframe);
			tfb_tx_make_current(tfb,ackframe);

			if (tfb_is_controller(tfb) && tfb->message_from_func)
				tfb->message_from_func(payload,payload_size,tfb_frame_get_num(tfb->rx_frame,TFB_FROM));

			if (tfb_is_device(tfb) && tfb->message_func)
				tfb->message_func(payload,payload_size);
		}

		if (tfb_frame_has_data(tfb->rx_frame,TFB_ACK)) {
			int ackseq=tfb_frame_get_num(tfb->rx_frame,TFB_ACK);
			//printf("got ack: %d we are: %d\n",ackseq,tfb->id);
			for (int i=0; i<tfb->tx_queue_len; i++) {
				if (tfb_frame_has_data(tfb->tx_queue[i],TFB_SEQ) &&
						tfb_frame_get_num(tfb->tx_queue[i],TFB_SEQ)==ackseq) {
					tfb_dispose_frame(tfb,tfb->tx_queue[i]);
					i--;
				}
			}
		}
	}

	if (tfb_is_controller(tfb) &&
			tfb_frame_has_data(tfb->rx_frame,TFB_ANNOUNCE_NAME)) {
		char *name=tfb_frame_get_strdup(tfb->rx_frame,TFB_ANNOUNCE_NAME);
		//printf("got announce name: %s\n",name);
		int id=tfb_get_device_id_by_name(tfb,name);
		if (!id) {
			char *type=tfb_frame_get_strdup(tfb->rx_frame,TFB_ANNOUNCE_TYPE);
			id=tfb_get_available_device_id(tfb);
			//printf("creating device id: %d\n",id);
			tfb_device_t *device=tfb_device_create(id,name,type);
			tfb_free(type);

			tfb->devices[tfb->num_devices++]=device;

			if (tfb->device_func)
				tfb->device_func(device->name);
		}

		tfb_frame_t *assignframe=tfb_frame_create(1024);
		tfb_frame_set_notification_func(assignframe,tfb,tfb_dispose_frame);
		tfb_frame_write_data(assignframe,TFB_ASSIGN_NAME,(uint8_t*)name,strlen(name));
		tfb_frame_write_num(assignframe,TFB_TO,id);
		tfb_frame_write_num(assignframe,TFB_SESSION_ID,tfb->session_id);
		tfb_frame_write_checksum(assignframe);
		tfb_tx_make_current(tfb,assignframe);

		tfb_free(name);
	}

	if (tfb_is_device(tfb) &&
			tfb_frame_has_data(tfb->rx_frame,TFB_SESSION_ID)) {
		int session_id=tfb_frame_get_num(tfb->rx_frame,TFB_SESSION_ID);
		// TODO! reset if session id changed...
		tfb->session_id=session_id;

		if (!tfb_frame_strcmp(tfb->rx_frame,TFB_ASSIGN_NAME,tfb->device_name)) {
			int id=tfb_frame_get_num(tfb->rx_frame,TFB_TO);
			//printf("got assignment: %d\n",id);
			if (id!=tfb->id) {
				tfb_set_id(tfb,id);
			}
		}
	}

	tfb_frame_reset(tfb->rx_frame);
}

bool tfb_tx_is_available(tfb_t *tfb) {
	if (!tfb->tx_frame)
		return false;

	return tfb_frame_tx_is_available(tfb->tx_frame);
}

uint8_t tfb_tx_pop_byte(tfb_t *tfb) {
	if (!tfb->tx_frame)
		return 0x7e;

	tfb_notify_bus_activity(tfb);

	uint8_t byte=tfb_frame_tx_pop_byte(tfb->tx_frame);
	if (!tfb_frame_tx_is_available(tfb->tx_frame)) {
		tfb_frame_notify(tfb->tx_frame);
		tfb->tx_frame=NULL;
	}

	return byte;
}

bool tfb_send(tfb_t *tfb, uint8_t *data, size_t size) {
	if (!tfb_is_device(tfb))
		return false;

	if (tfb->tx_queue_len>=TFB_TX_QUEUE_LEN)
		return false;

	tfb_frame_t *frame=tfb_frame_create(size+128);
	tfb_frame_write_num(frame,TFB_FROM,tfb->id);
	tfb_frame_write_num(frame,TFB_SEQ,tfb->seq);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,size);
	tfb_frame_write_checksum(frame);
	tfb_frame_set_notification_func(frame,tfb,tfb_schedule_resend);
	tfb->seq++;
	tfb->tx_queue[tfb->tx_queue_len++]=frame;

	//printf("here... qlen=%d\n",tfb->tx_queue_len);

	return true;
}

bool tfb_send_to(tfb_t *tfb, uint8_t *data, size_t size, int to) {
	if (!tfb_is_controller(tfb))
		return false;

	if (tfb->tx_queue_len>=TFB_TX_QUEUE_LEN)
		return false;

	tfb_frame_t *frame=tfb_frame_create(size+128);
	tfb_frame_write_num(frame,TFB_TO,to);
	tfb_frame_write_num(frame,TFB_SEQ,tfb->seq);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,size);
	tfb_frame_write_checksum(frame);
	tfb_frame_set_notification_func(frame,tfb,tfb_schedule_resend);
	tfb->seq++;
	tfb->tx_queue[tfb->tx_queue_len++]=frame;

	return true;
}

tfb_frame_t *tfb_get_send_frame(tfb_t *tfb) {
	int millis=tfb_millis();
	for (int i=0; i<tfb->tx_queue_len; i++)
		if (millis>=tfb->tx_queue[i]->send_at)
			return tfb->tx_queue[i];

	return NULL;
}

void tfb_tick(tfb_t *tfb) {
	if (tfb->tx_frame)
		return;

	int millis=tfb_millis();
	if (tfb->announcement_deadline &&
			millis>=tfb->announcement_deadline) {
		tfb->announcement_deadline=0;
		if (tfb_is_controller(tfb)) {
			tfb->tx_queue[tfb->tx_queue_len++]=tfb_create_announce_session_frame(tfb);
		}

		/*if (tfb_is_device(tfb)) {
		}*/
	}

	for (int i=0; i<tfb->tx_queue_len; i++) {
		if (tfb->tx_queue[i]->sent_times>=TFB_RETRIES &&
				tfb->tx_queue[i]->send_at>=millis) {
			tfb_dispose_frame(tfb,tfb->tx_queue[i]);
			i--;
		}
	}

	tfb_frame_t *send_cand=tfb_get_send_frame(tfb);
	if (tfb_is_bus_available(tfb) &&
			!tfb->tx_frame &&
			send_cand) {
		tfb_tx_make_current(tfb,send_cand);
	}
}

int tfb_get_queue_len(tfb_t *tfb) {
	return tfb->tx_queue_len;
}

void tfb_millis_func(uint32_t (*func)()) {
	tfb_millis=func;
}

void tfb_notify_bus_activity(tfb_t *tfb) {
	uint32_t baud=9600;
	uint32_t byte_time = 1000 * 10 / baud; // 10 bits per byte in ms
	if (!byte_time)
		byte_time=1;

	uint32_t base = 2 * byte_time;         // wait at least 2 bytes
	uint32_t random_backoff = (rand() % 10);// * byte_time;

	tfb->bus_available_millis=tfb_millis()+base+random_backoff;
	//printf("update bus available: %u\n",tfb->bus_available_millis);
}

bool tfb_is_bus_available(tfb_t *tfb) {
	if ((int32_t)(tfb_millis() - tfb->bus_available_millis) >= 0)
		return true;

	return false;
}

void tfb_srand(unsigned int seed) {
	srand(seed);
}

int tfb_get_timeout(tfb_t *tfb) {
	int deadline=0;

	if (tfb->announcement_deadline)
		deadline=tfb->announcement_deadline;

	for (int i=0; i<tfb->tx_queue_len; i++) {
		if (!deadline || tfb->tx_queue[i]->send_at<deadline)
			deadline=tfb->tx_queue[i]->send_at;
	}

	if (!deadline)
		return -1;

	if (deadline<tfb->bus_available_millis)
		deadline=tfb->bus_available_millis;

	int delay=deadline-tfb_millis();
	if (delay<0)
		delay=0;

	return delay;
}
