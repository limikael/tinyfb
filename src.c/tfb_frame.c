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
	frame->resend_count=0;
	frame->deadline=TFB_TIME_NEVER;
	frame->submission_number=0;

	return frame;
}

/*void tfb_frame_update_resend_deadline(tfb_frame_t *frame) {
	frame->resend_deadline=tfb_time_future(TFB_RESEND_BASE<<(frame->resend_count));
}*/

tfb_frame_t *tfb_frame_create_from_data(uint8_t *data, size_t size) {
	tfb_frame_t *frame=tfb_frame_create(size);
	memcpy(frame->buffer,data,size);
	frame->size=size;

	return frame;
}

void tfb_frame_dispose(tfb_frame_t *frame) {
	tfb_free(frame->buffer);
	tfb_free(frame);
}

size_t tfb_frame_get_size(tfb_frame_t *frame) {
	return frame->size;
}

uint8_t *tfb_frame_get_buffer(tfb_frame_t *frame) {
	return frame->buffer;
}

uint8_t tfb_frame_get_buffer_at(tfb_frame_t *frame, size_t index) {
	return frame->buffer[index];
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
			case TFB_PAYLOAD: keyname="payload"; isdata=1; break;
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

void tfb_frame_reset(tfb_frame_t *frame) {
	frame->size=0;
}
