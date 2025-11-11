#include "tfb_frame.h"
#include "tfb.h"
#include "tfb_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void tokenize_frame_buffer(uint8_t **buffer, uint8_t *key, uint8_t **data, size_t *size) {
	*key=(**buffer)>>3;
	int sizecode=(**buffer)&7;
	(*buffer)++;

	switch (sizecode) {
		case 1:
			*size=1;
			break;

		case 2:
			*size=2;
			break;

		case 3:
			*size=4;
			break;

		case 5:
			(*size)=(**buffer);
			(*buffer)++;
			break;

		case 6:
			*size=((*buffer)[0]<<8)+(*buffer)[1];
			(*buffer)+=2;
			break;

		default:
			*size=0;
			return;
			break;
	}

	(*data)=(*buffer);
	(*buffer)+=*size;
}

tfb_frame_t *tfb_frame_create(size_t capacity) {
	tfb_frame_t *frame=tfb_malloc(sizeof(tfb_frame_t));
	frame->buffer=tfb_malloc(capacity);
	frame->capacity=capacity;
	frame->size=0;
	frame->rx_state=RX_RECEIVE;
	frame->tx_state=TX_INIT;
	frame->tx_index=0;
	frame->send_at=tfb_millis();
	frame->sent_times=0;
	frame->tfb=NULL;
	frame->notification_func=NULL;

	return frame;
}

void tfb_frame_set_notification_func(tfb_frame_t *frame, tfb_t *tfb, void (*func)(tfb_t *tfb, tfb_frame_t *tfb_frame)) {
	frame->tfb=tfb;
	frame->notification_func=func;
}

void tfb_frame_notify(tfb_frame_t *frame) {
	if (!frame->notification_func)
		return;

	frame->notification_func(frame->tfb,frame);
}

void tfb_frame_dispose(tfb_frame_t *frame) {
	tfb_free(frame->buffer);
	tfb_free(frame);
}

void tfb_frame_rx_push_byte(tfb_frame_t *frame, uint8_t byte) {
	if (frame->rx_state==RX_COMPLETE)
		return;

	if (frame->rx_state==RX_ESCAPE) {
		frame->buffer[frame->size++]=byte^0x20;
		frame->rx_state=RX_RECEIVE;
		return;
	}

	if (byte==0x7d) {
		frame->rx_state=RX_ESCAPE;
		return;
	}

	if (byte==0x7e) {
		if (frame->size)
			frame->rx_state=RX_COMPLETE;

		return;
	}

	frame->buffer[frame->size++]=byte;
}

bool tfb_frame_rx_is_complete(tfb_frame_t *frame) {
	return (frame->rx_state==RX_COMPLETE);
}

size_t tfb_frame_get_size(tfb_frame_t *frame) {
	return frame->size;
}

uint8_t tfb_frame_get_buffer_at(tfb_frame_t *frame, size_t index) {
	return frame->buffer[index];
}

void tfb_frame_reset(tfb_frame_t *frame) {
	tfb_frame_tx_rewind(frame);
	frame->size=0;
	frame->rx_state=RX_RECEIVE;
}

bool tfb_frame_tx_is_available(tfb_frame_t *frame) {
	if (!frame->size)
		return false;

	if (frame->tx_state==TX_COMPLETE)
		return false;

	return true;
}

uint8_t tfb_frame_tx_pop_byte(tfb_frame_t *frame) {
	if (!tfb_frame_tx_is_available(frame))
		return 0x7e;

	switch (frame->tx_state) {
		case TX_INIT:
			frame->tx_state=TX_SENDING;
			return 0x7e;
			break;

		case TX_ESCAPING:
			frame->tx_state=TX_SENDING;
			return (0x20^frame->buffer[frame->tx_index++]);
			break;

		case TX_SENDING:
			if (frame->tx_index>=frame->size) {
				frame->tx_state=TX_COMPLETE;
				return 0x7e;
			}

			if (frame->buffer[frame->tx_index]==0x7d ||
					frame->buffer[frame->tx_index]==0x7e) {
				frame->tx_state=TX_ESCAPING;
				return 0x7d;
			}

			return frame->buffer[frame->tx_index++];
			break;

		case TX_COMPLETE:
			return 0x7e;
	}
}

void tfb_frame_tx_rewind(tfb_frame_t *frame) {
	frame->tx_state=TX_INIT;
	frame->tx_index=0;
}

void tfb_frame_write_byte(tfb_frame_t *frame, uint8_t byte) {
	frame->buffer[frame->size++]=byte;
}

void tfb_frame_write_data(tfb_frame_t *frame, uint8_t key, uint8_t *bytes, size_t size) {
	if (!size || size>=65535)
		return;

	if (size==1) {
		tfb_frame_write_byte(frame,(key<<3)|1);
	}

	else if (size==2) {
		tfb_frame_write_byte(frame,(key<<3)|2);
	}

	else if (size==4) {
		tfb_frame_write_byte(frame,(key<<3)|3);
	}

	else if (size<=255) {
		tfb_frame_write_byte(frame,(key<<3)|5);
		tfb_frame_write_byte(frame,size);
	}

	else {
		tfb_frame_write_byte(frame,(key<<3)|6);
        tfb_frame_write_byte(frame, (uint8_t)((size >> 8) & 0xFF));
        tfb_frame_write_byte(frame, (uint8_t)(size & 0xFF));
	}

	for (size_t i=0; i<size; i++)
		tfb_frame_write_byte(frame,bytes[i]);
}

size_t tfb_frame_get_data_size(tfb_frame_t *frame, uint8_t key) {
	uint8_t datakey,*data,*buffer;
	size_t size;

	for (buffer=frame->buffer; buffer<frame->buffer+frame->size; /**/) {
		tokenize_frame_buffer(&buffer,&datakey,&data,&size);
		if (!size)
			return 0;

		if (key==datakey)
			return size;
	}

	return 0;
}

uint8_t *tfb_frame_get_data(tfb_frame_t *frame, uint8_t key) {
	uint8_t datakey,*data,*buffer;
	size_t size;

	for (buffer=frame->buffer; buffer<frame->buffer+frame->size;) {
		tokenize_frame_buffer(&buffer,&datakey,&data,&size);
		if (!size)
			return 0;

		if (key==datakey)
			return data;
	}

	return 0;
}

void tfb_frame_write_num(tfb_frame_t *frame, uint8_t key, int num) {
	if (!key)
		return;

    if (num >= -128 && num <= 127) {
    	int8_t num8=num;
    	tfb_frame_write_data(frame,key,(uint8_t *)&num8,1);
    }

    else {
		uint16_t num16=(uint16_t)(num&0xFFFF);
		uint8_t data[2];
		data[0]=(num16>>8)&0xFF;
		data[1]=(num16>>0)&0xFF;
    	tfb_frame_write_data(frame,key,data,2);
    }
}

char *tfb_frame_get_strdup(tfb_frame_t *frame, uint8_t key) {
	size_t size=tfb_frame_get_data_size(frame,key);
	uint8_t *data=tfb_frame_get_data(frame,key);

	char *s=tfb_malloc(size+1);
	memcpy(s,data,size);
	s[size]='\0';

	return s;
}

int tfb_frame_get_num(tfb_frame_t *frame, uint8_t key) {
	size_t size=tfb_frame_get_data_size(frame,key);
	uint8_t *data=tfb_frame_get_data(frame,key);
	switch (size) {
		case 1:
			return (int)(*(int8_t*)data);
			break;

		case 2:
			return (int)(int16_t)(((uint16_t)data[0]<<8)|data[1]);
			break;

		default:
			return 0;
	}
}

uint8_t tfb_frame_get_checksum(tfb_frame_t *frame) {
	uint8_t check=0;
	for (int i=0; i<frame->size; i++)
		check^=frame->buffer[i];

	return check;
}

void tfb_frame_write_checksum(tfb_frame_t *frame) {
	uint8_t checksum=tfb_frame_get_checksum(frame);

	checksum^=(TFB_CHECKSUM<<3)+1;

	tfb_frame_write_data(frame,TFB_CHECKSUM,&checksum,1);
}

bool tfb_frame_has_data(tfb_frame_t *frame, uint8_t key) {
	return (tfb_frame_get_data_size(frame,key)>0);
}

int tfb_frame_get_key_at(tfb_frame_t *frame, int index) {
	uint8_t datakey,*data,*buffer;
	size_t size;
	int keyindex=0;

	for (buffer=frame->buffer; buffer<frame->buffer+frame->size;) {
		tokenize_frame_buffer(&buffer,&datakey,&data,&size);
		if (keyindex==index)
			return datakey;

		keyindex++;
	}

	return -1;
}

int tfb_frame_get_num_keys(tfb_frame_t *frame) {
	uint8_t datakey,*data,*buffer;
	size_t size;
	int keyindex=0;

	for (buffer=frame->buffer; buffer<frame->buffer+frame->size;) {
		tokenize_frame_buffer(&buffer,&datakey,&data,&size);
		keyindex++;
	}

	return keyindex;
}

char *tfb_frame_sprint(tfb_frame_t *frame, char *s) {
	char *p=s;
	for (int i=0; i<tfb_frame_get_num_keys(frame); i++) {
		int key=tfb_frame_get_key_at(frame,i);
		char *keyname=NULL;
		int isdata=0;

		switch (key) {
			case TFB_CHECKSUM: keyname="checksum"; break;
			case TFB_FROM: keyname="from"; break;
			case TFB_TO: keyname="to"; break;
			case TFB_PAYLOAD: keyname="payload"; break;
			case TFB_SEQ: keyname="seq"; break;
			case TFB_ACK: keyname="ack"; break;
			case TFB_ANNOUNCE_NAME: keyname="announce_name"; isdata=1; break;
			case TFB_ANNOUNCE_TYPE: keyname="announce_type"; isdata=1; break;
			case TFB_ASSIGN_NAME: keyname="assign_name"; isdata=1; break;
			case TFB_SESSION_ID: keyname="session_id"; break;
		}

		if (isdata) {
			p+=sprintf(p,"%s: (%zu) '",keyname,tfb_frame_get_data_size(frame,key));

			for (int j=0; j<tfb_frame_get_data_size(frame,key); j++) {
				p+=sprintf(p,"%c",tfb_frame_get_data(frame,key)[j]);
			}

			p+=sprintf(p,"' ");
		}

		else {
			p+=sprintf(p,"%s: %d ",keyname,tfb_frame_get_num(frame,key));
		}
	}

	return s;
}

int tfb_frame_strcmp(tfb_frame_t *frame, uint8_t key, char *s) {
	if (!tfb_frame_has_data(frame,key))
		return -1;

	size_t size=tfb_frame_get_data_size(frame,key);
	if (size!=strlen(s))
		return -1;

	uint8_t *data=tfb_frame_get_data(frame,key);
	for (int i=0; i<size; i++)
		if (s[i]!=data[i])
			return -1;

	return 0;
}
