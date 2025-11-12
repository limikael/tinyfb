#include "tfb.h"
#include "tfb_frame.h"
#include "tfb_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

uint32_t (*tfb_millis)()=NULL;

int tfb_get_errno(tfb_t *tfb) {
	return tfb->errno;
}

int tfb_get_session_id(tfb_t *tfb) {
	return tfb->session_id;
}

void tfb_dispose_frame(tfb_t *tfb, tfb_frame_t *frame) {
	//printf("dispose frame called!!!\n");

	for (int i=tfb->tx_queue_len-1; i>=0; i--)
		if (tfb->tx_queue[i]==frame)
			pointer_array_remove((void**)tfb->tx_queue,&tfb->tx_queue_len,i);

	if (tfb->tx_frame==frame)
		tfb->tx_frame=NULL;

	tfb_frame_dispose(frame);
}

int tfb_get_device_id_by_name(tfb_t *tfb, char *name) {
	if (!tfb_is_controller(tfb))
		return 0;

	for (int i=0; i<tfb->num_devices; i++)
		if (!strcmp(name,tfb->devices[i]->name))
			return tfb->devices[i]->id;

	return 0;
}

tfb_device_t *tfb_get_device_by_id(tfb_t *tfb, int id) {
	if (!tfb_is_controller(tfb) || !id)
		return 0;

	for (int i=0; i<tfb->num_devices; i++)
		if (id==tfb->devices[i]->id)
			return tfb->devices[i];

	return NULL;
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
	tfb->status_func=NULL;
	tfb->tx_queue_len=0;
	tfb->bus_available_millis=0;
	tfb->outseq=0;
	tfb->inseq=0;
	tfb->device_name=NULL;
	tfb->device_type=NULL;
	tfb->announcement_deadline=0;
	tfb->activity_deadline=0;
	tfb->num_devices=0;
	tfb->devices=NULL;
	tfb->session_id=0;
	tfb->device_func=NULL;
	tfb->rx_deliverable=false;

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

void tfb_status_func(tfb_t *tfb, void (*func)()) {
	tfb->status_func=func;
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
	if (!tfb_is_device(tfb))
		return;

	if (id==tfb->id)
		return;

	if (tfb->rx_deliverable) {
		tfb->rx_deliverable=false;
		tfb_frame_reset(tfb->rx_frame);
	}

	bool prev_connected=tfb_is_connected(tfb);
	if (tfb->tx_frame)
		tfb_dispose_frame(tfb,tfb->tx_frame);

	while (tfb->tx_queue_len)
		tfb_dispose_frame(tfb,tfb->tx_queue[0]);

	tfb->id=id;
	if (!tfb_is_connected(tfb)) {
		tfb->activity_deadline=0;
		tfb->inseq=0;
		tfb->outseq=0;
	}

	if (tfb_is_connected(tfb)) {
		tfb->inseq=1;
		tfb->outseq=1;
	}

	bool current_connected=tfb_is_connected(tfb);

	if (prev_connected!=current_connected &&
			tfb->status_func)
		tfb->status_func();
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
				tfb_frame_has_data(frame,TFB_FROM) &&
				tfb_get_device_by_id(tfb,tfb_frame_get_num(frame,TFB_FROM)))
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
	if (!tfb_rx_is_available(tfb))
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
			tfb_frame_t *ackframe=NULL;

			if (tfb_is_controller(tfb)) {
				tfb_device_t *device=tfb_get_device_by_id(tfb,tfb_frame_get_num(tfb->rx_frame,TFB_FROM));
				if (device &&
						tfb_frame_get_num(tfb->rx_frame,TFB_SEQ)<=device->inseq) {
					ackframe=tfb_frame_create(128);
					tfb_frame_write_num(ackframe,TFB_TO,device->id);

					//printf("acking in controller to: %d\n",tfb_frame_get_num(tfb->rx_frame,TFB_FROM));
					if (tfb_frame_get_num(tfb->rx_frame,TFB_SEQ)==device->inseq) {
						tfb->rx_deliverable=true;
						device->inseq++;
					}
				}
			}

			if (tfb_is_device(tfb) &&
					tfb_frame_get_num(tfb->rx_frame,TFB_SEQ)<=tfb->inseq) {
				ackframe=tfb_frame_create(128);
				tfb_frame_write_num(ackframe,TFB_FROM,tfb->id);

				//printf("got: %d, expected: %d\n",tfb_frame_get_num(tfb->rx_frame,TFB_SEQ),tfb->inseq);
				if (tfb_frame_get_num(tfb->rx_frame,TFB_SEQ)==tfb->inseq) {
					tfb->rx_deliverable=true;
					tfb->inseq++;
				}
			}

			if (ackframe) {
				tfb_frame_set_notification_func(ackframe,tfb,tfb_dispose_frame);
				tfb_frame_write_num(ackframe,TFB_ACK,tfb_frame_get_num(tfb->rx_frame,TFB_SEQ));
				tfb_frame_write_checksum(ackframe);
				tfb_tx_make_current(tfb,ackframe);
			}
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
			tfb_is_connected(tfb) &&
			tfb_frame_get_num(tfb->rx_frame,TFB_RESET_TO)==tfb->id) {
		tfb_set_id(tfb,-1);
	}

	if (tfb_is_device(tfb) &&
			tfb_frame_has_data(tfb->rx_frame,TFB_SESSION_ID)) {
		int session_id=tfb_frame_get_num(tfb->rx_frame,TFB_SESSION_ID);

		if (session_id!=tfb->session_id) {
			//printf("session reset: %d\n",session_id);
			tfb_set_id(tfb,-1);
			tfb->session_id=session_id;
		}

		if (!tfb_frame_strcmp(tfb->rx_frame,TFB_ASSIGN_NAME,tfb->device_name)) {
			int id=tfb_frame_get_num(tfb->rx_frame,TFB_TO);
			//printf("got assignment: %d\n",id);
			if (id!=tfb->id) {
				tfb_set_id(tfb,id);
			}
		}

		if (tfb_is_connected(tfb) &&
				!tfb_frame_has_data(tfb->rx_frame,TFB_ASSIGN_NAME)) {
			tfb->activity_deadline=tfb_millis()+TFB_CONNECTION_TIMEOUT;
			tfb_frame_t *pongframe=tfb_frame_create(32);
			tfb_frame_set_notification_func(pongframe,tfb,tfb_dispose_frame);
			tfb_frame_write_num(pongframe,TFB_FROM,tfb->id);
			tfb_frame_write_checksum(pongframe);
			tfb->tx_queue[tfb->tx_queue_len++]=pongframe;
		}

		if (!tfb_is_connected(tfb)) {
			tfb->tx_queue[tfb->tx_queue_len++]=tfb_create_announce_device_frame(tfb);
		}
	}

	if (tfb_is_controller(tfb) &&
			tfb_frame_has_data(tfb->rx_frame,TFB_FROM)) {
		int from_id=tfb_frame_get_num(tfb->rx_frame,TFB_FROM);
		tfb_device_t *device=tfb_get_device_by_id(tfb,from_id);
		if (device) {
			device->activity_deadline=tfb_millis()+TFB_CONNECTION_TIMEOUT;
		}

		else {
			tfb_frame_t *resetframe=tfb_frame_create(1024);
			tfb_frame_set_notification_func(resetframe,tfb,tfb_dispose_frame);
			tfb_frame_write_num(resetframe,TFB_SESSION_ID,tfb->session_id);
			tfb_frame_write_num(resetframe,TFB_RESET_TO,from_id);
			tfb_frame_write_checksum(resetframe);
			tfb->tx_queue[tfb->tx_queue_len++]=resetframe;
		}
	}

	if (!tfb->rx_deliverable)
		tfb_frame_reset(tfb->rx_frame);
}

bool tfb_rx_is_available(tfb_t *tfb) {
	if (tfb->rx_deliverable)
		return false;

	if (tfb->tx_frame)
		return false;

	return true;
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
	tfb->errno=0;

	if (!tfb_is_device(tfb)) {
		tfb->errno=TFB_EOPNOTSUPP;
		return false;
	}

	if (tfb->tx_queue_len>=TFB_TX_QUEUE_LEN) {
		tfb->errno=TFB_ENOSPC;
		return false;
	}

	if (!tfb_is_connected(tfb)) {
		tfb->errno=TFB_ENOTCONN;
		return false;
	}

	tfb_frame_t *frame=tfb_frame_create(size+128);
	tfb_frame_write_num(frame,TFB_FROM,tfb->id);
	tfb_frame_write_num(frame,TFB_SEQ,tfb->outseq++);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,size);
	tfb_frame_write_checksum(frame);
	tfb_frame_set_notification_func(frame,tfb,tfb_schedule_resend);
	tfb->tx_queue[tfb->tx_queue_len++]=frame;

	//printf("here... qlen=%d\n",tfb->tx_queue_len);

	return true;
}

bool tfb_send_to(tfb_t *tfb, uint8_t *data, size_t size, int to) {
	if (!tfb_is_controller(tfb)) {
		tfb->errno=TFB_EOPNOTSUPP;
		return false;
	}

	if (tfb->tx_queue_len>=TFB_TX_QUEUE_LEN) {
		tfb->errno=TFB_ENOSPC;
		return false;
	}

	tfb_device_t *device=tfb_get_device_by_id(tfb,to);
	if (!device) {
		tfb->errno=TFB_ENOTCONN;
		return false;
	}

	tfb_frame_t *frame=tfb_frame_create(size+128);
	tfb_frame_write_num(frame,TFB_TO,to);
	tfb_frame_write_num(frame,TFB_SEQ,device->outseq++);
	tfb_frame_write_data(frame,TFB_PAYLOAD,data,size);
	tfb_frame_write_checksum(frame);
	tfb_frame_set_notification_func(frame,tfb,tfb_schedule_resend);
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

tfb_frame_t *tfb_get_expired_frame(tfb_t *tfb) {
	int millis=tfb_millis();
	for (int i=0; i<tfb->tx_queue_len; i++)
		if (tfb->tx_queue[i]->sent_times>=TFB_RETRIES &&
				tfb->tx_queue[i]->send_at>=millis)
			return tfb->tx_queue[i];

	return NULL;
}

void tfb_close_device(tfb_t *tfb, tfb_device_t *device) {
	for (int i=tfb->tx_queue_len-1; i>=0; i--)
		if (tfb_frame_get_num(tfb->tx_queue[i],TFB_TO)==device->id)
			tfb_dispose_frame(tfb,tfb->tx_queue[i]);

	for (int i=tfb->num_devices-1; i>=0; i--)
		if (tfb->devices[i]==device)
			pointer_array_remove((void**)tfb->devices,&tfb->num_devices,i);

	if (tfb->device_func)
		tfb->device_func(device->name);

	tfb_device_dispose(device);
}

void tfb_tick(tfb_t *tfb) {
	if (tfb->tx_frame)
		return;

	if (tfb->rx_deliverable) {
		uint8_t *payload=tfb_frame_get_data(tfb->rx_frame,TFB_PAYLOAD);
		size_t payload_size=tfb_frame_get_data_size(tfb->rx_frame,TFB_PAYLOAD);

		if (tfb_is_controller(tfb) && tfb->message_from_func)
			tfb->message_from_func(payload,payload_size,tfb_frame_get_num(tfb->rx_frame,TFB_FROM));

		if (tfb_is_device(tfb) && tfb->message_func)
			tfb->message_func(payload,payload_size);

		tfb->rx_deliverable=false;
		tfb_frame_reset(tfb->rx_frame);
	}


	int millis=tfb_millis();
	if (tfb->announcement_deadline &&
			millis>=tfb->announcement_deadline) {
		tfb->announcement_deadline=0;
		tfb->tx_queue[tfb->tx_queue_len++]=tfb_create_announce_session_frame(tfb);
	}

	if (tfb_is_device(tfb) &&
			tfb_get_expired_frame(tfb)) {
		tfb_set_id(tfb,-1);
		return;
	}

	if (tfb_is_controller(tfb)) {
		while (tfb_get_expired_frame(tfb)) {
			tfb_frame_t *frame=tfb_get_expired_frame(tfb);
			tfb_device_t *device=tfb_get_device_by_id(tfb,tfb_frame_get_num(frame,TFB_TO));
			if (device) {
				tfb_close_device(tfb,device);
			}

			else {
				tfb_dispose_frame(tfb,frame);
			}
		}

		for (int i=tfb->num_devices-1; i>=0; i--)
			if (millis>=tfb->devices[i]->activity_deadline)
				tfb_close_device(tfb,tfb->devices[i]);
	}

	tfb_frame_t *send_cand=tfb_get_send_frame(tfb);
	if (tfb_is_bus_available(tfb) &&
			!tfb->tx_frame &&
			send_cand) {
		tfb_tx_make_current(tfb,send_cand);
	}

	if (tfb->activity_deadline && millis>=tfb->activity_deadline) {
		tfb_set_id(tfb,-1);
		return;
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

	if (tfb->activity_deadline)
		if (!deadline || tfb->activity_deadline<deadline)
			deadline=tfb->activity_deadline;

	for (int i=0; i<tfb->tx_queue_len; i++)
		if (!deadline || tfb->tx_queue[i]->send_at<deadline)
			deadline=tfb->tx_queue[i]->send_at;

	for (int i=0; i<tfb->num_devices; i++)
		if (!deadline || tfb->devices[i]->activity_deadline<deadline)
			deadline=tfb->devices[i]->activity_deadline;

	if (!deadline)
		return -1;

	if (deadline<tfb->bus_available_millis)
		deadline=tfb->bus_available_millis;

	int delay=deadline-tfb_millis();
	if (delay<0)
		delay=0;

	return delay;
}
