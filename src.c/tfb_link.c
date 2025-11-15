#include "tfb_internal.h"
#include "tfb_util.h"
#include <string.h>
#include <stdio.h>

#define TFB_RXBUFSIZE 1024
//#define TFB_TXQUEUESIZE 8

tfb_link_t *tfb_link_create() {
	tfb_link_t *link=tfb_malloc(sizeof(tfb_link_t));
	link->rx_buf=tfb_malloc(TFB_RXBUFSIZE);
	link->rx_pos=0;
	link->rx_state=TFB_LINK_RX_INIT;
	link->tx_state=TFB_LINK_TX_IDLE;
	link->frame_func=NULL;
	link->tx_queue_len=0;
	link->tx_index=0;
	link->tag=NULL;
	link->num_submitted=0;

	tfb_link_notify_bus_activity(link);

	return link;
}

void tfb_link_set_tag(tfb_link_t *link, void *tag) {
	link->tag=tag;
}

void *tfb_link_get_tag(tfb_link_t *link) {
	return link->tag;
}

void tfb_link_notify_bus_activity(tfb_link_t *link) {
	int delay=TFB_CA_DELAY_MIN+(rand()%(TFB_CA_DELAY_MAX-TFB_CA_DELAY_MIN));
	link->bus_available_deadline=tfb_time_future(delay);

	/*uint32_t baud=9600;
	uint32_t byte_time = 1000 * 10 / baud; // 10 bits per byte in ms
	if (!byte_time)
		byte_time=1;

	uint32_t base = 2 * byte_time;         // wait at least 2 bytes
	uint32_t random_backoff = (rand() % 10);// * byte_time;

	link->bus_available_millis=tfb_time_now()+base+random_backoff;
	//printf("update bus available: %u\n",tfb->bus_available_millis);*/
}

void tfb_link_dispose(tfb_link_t *link) {
	for (int i=0; i<link->tx_queue_len; i++)
		tfb_free(link->tx_queue[i].data);

	//tfb_free(link->tx_queue);
	tfb_free(link->rx_buf);
	tfb_free(link);
}

void tfb_link_frame_func(tfb_link_t *link, void (*func)(tfb_link_t *link, uint8_t *data, size_t size)) {
	link->frame_func=func;
}

void tfb_link_rx_push_byte(tfb_link_t *link, uint8_t byte) {
	tfb_link_notify_bus_activity(link);

	if (link->rx_state==TFB_LINK_RX_INIT) {
		if (byte==0x7e)
			link->rx_state=TFB_LINK_RX_RECEIVE;

		return;
	}

	switch (byte) {
		case 0x7e:
			if (link->rx_pos &&
					link->frame_func &&
					!compute_xor_checksum(link->rx_buf,link->rx_pos))
				link->frame_func(link,link->rx_buf,link->rx_pos);

			link->rx_state=TFB_LINK_RX_RECEIVE;
			link->rx_pos=0;
			break;

		case 0x7d:
			link->rx_state=TFB_LINK_RX_ESCAPE;
			break;

		default:
			if (link->rx_state==TFB_LINK_RX_ESCAPE)
				link->rx_buf[link->rx_pos++]=byte^0x20;

			else
				link->rx_buf[link->rx_pos++]=byte;

			link->rx_state=TFB_LINK_RX_RECEIVE;
			break;
	}
}

bool tfb_link_send(tfb_link_t *link, uint8_t *data, size_t size) {
	if (compute_xor_checksum(data,size))
		return false;

	//printf("sending link bytes: %zu... queue size: %d\n",size,link->tx_queue_len);

	link->tx_queue[link->tx_queue_len].data=tfb_malloc(size);
	memcpy(link->tx_queue[link->tx_queue_len].data,data,size);
	link->tx_queue[link->tx_queue_len].size=size;
	link->tx_queue_len++;
	link->num_submitted++;

	return true;
}

bool tfb_link_tx_is_available(tfb_link_t *link) {
	if (!link->tx_queue_len)
		return false;

	if (link->tx_state!=TFB_LINK_TX_IDLE)
		return true;

	if (tfb_link_get_timeout(link)>0)
		return false;

	return true;
}

tfb_time_t tfb_link_get_deadline(tfb_link_t *link) {
	if (link->tx_state!=TFB_LINK_TX_IDLE)
		return tfb_time_now();

	if (!link->tx_queue_len)
		return TFB_TIME_NEVER;

	return link->bus_available_deadline;
}

int tfb_link_get_timeout(tfb_link_t *link) {
	return tfb_time_timeout(tfb_link_get_deadline(link));
}

uint8_t tfb_link_tx_pop_byte(tfb_link_t *link) {
	tfb_link_notify_bus_activity(link);

	if (!link->tx_queue_len)
		return 0x7e;

	if (link->tx_state==TFB_LINK_TX_IDLE) {
		link->tx_state=TFB_LINK_TX_SENDING;
		return 0x7e;
	}

	if (link->tx_index>=link->tx_queue[0].size) {
		link->tx_state=TFB_LINK_TX_IDLE;
		tfb_free(link->tx_queue[0].data);
		memmove(&link->tx_queue[0],&link->tx_queue[1],sizeof(link->tx_queue[0])*link->tx_queue_len-1);
		link->tx_queue_len--;
		link->tx_index=0;
		return 0x7e;
	}

	uint8_t byte=link->tx_queue[0].data[link->tx_index];
	if (link->tx_state==TFB_LINK_TX_ESCAPE) {
		link->tx_state=TFB_LINK_TX_SENDING;
		link->tx_index++;
		return (byte^0x20);
	}

	if (byte==0x7e || byte==0x7d) {
		link->tx_state=TFB_LINK_TX_ESCAPE;
		return 0x7d;
	}

	link->tx_index++;
	return byte;
}

uint32_t tfb_link_get_num_submitted(tfb_link_t *link) {
	return link->num_submitted;
}

uint32_t tfb_link_get_num_transmitted(tfb_link_t *link) {
	return (link->num_submitted-link->tx_queue_len);
}

//bool tfb_link_is_transmitted(tfb_link_t *link, uint32_t submitted);

bool tfb_link_is_transmitted(tfb_link_t *link, uint32_t submitted) {
    uint32_t num_transmitted = link->num_submitted - link->tx_queue_len;

    // True if (num_transmitted >= submitted) in modulo-2^32 arithmetic
    return (uint32_t)(num_transmitted - submitted) < 0x80000000u;
}
