#include "tfb_link.h"
#include "tfb_physical.h"
#include "tfb_util.h"
#include <stdio.h>
#include <string.h>

tfb_link_t *tfb_link_create(tfb_physical_t *physical) {
	tfb_link_t *link=tfb_malloc(sizeof(tfb_link_t));

	link->physical=physical;
	link->rx_size=0;
	link->rx_state=TFB_LINK_RX_INIT;
	link->tx_size=0;

	tfb_link_notify_bus_activity(link);

	return link;
}

void tfb_link_dispose(tfb_link_t *link) {
	tfb_free(link);
}

bool tfb_link_write_frame(tfb_link_t *link, uint8_t *data, size_t size) {
	//printf(">! writing frame, size=%zu\n",size);

	tfb_physical_write(link->physical,0x7e);

	for (int i=0; i<size; i++) {
		uint8_t byte=data[i];

		if (byte==0x7e || byte==0x7d) {
			tfb_physical_write(link->physical,0x7d);
			tfb_physical_write(link->physical,0x20^byte);
		}

		else {
			tfb_physical_write(link->physical,byte);
		}
	}

	tfb_physical_write(link->physical,0x7e);

	tfb_link_notify_bus_activity(link);

	return true;
}

void tfb_link_handle_rx_byte(tfb_link_t *link, uint8_t byte) {
	//printf("handle byte: %d pos: %d\n",byte,link->rx_pos);

	tfb_link_notify_bus_activity(link);
	if (link->rx_state==TFB_LINK_RX_COMPLETE)
		return;

	if (link->rx_state==TFB_LINK_RX_INIT) {
		if (byte==0x7e)
			link->rx_state=TFB_LINK_RX_RECEIVE;

		return;
	}

	switch (byte) {
		case 0x7e:
			if (link->rx_size &&
					!compute_xor_checksum(link->rx_buf,link->rx_size)) {
				link->rx_state=TFB_LINK_RX_COMPLETE;
				return;
			}

			link->rx_state=TFB_LINK_RX_RECEIVE;
			link->rx_size=0;
			//printf("seen 0x7e... pos=%d\n",link->rx_pos);
			break;

		case 0x7d:
			link->rx_state=TFB_LINK_RX_ESCAPE;
			break;

		default:
			if (link->rx_size<TFB_BUFSIZE) {
				if (link->rx_state==TFB_LINK_RX_ESCAPE)
					link->rx_buf[link->rx_size++]=byte^0x20;

				else
					link->rx_buf[link->rx_size++]=byte;
			}

			link->rx_state=TFB_LINK_RX_RECEIVE;
			break;
	}
}

void tfb_link_tick(tfb_link_t *link) {
	int available=tfb_physical_available(link->physical);
	//printf("av before: %d\n",available);

	for (int i=0; i<available; i++) 
		tfb_link_handle_rx_byte(link,tfb_physical_read(link->physical));

	available=tfb_physical_available(link->physical);
	//printf("av after: %d\n",available);

	//printf("** tick: %p available=%d qlen=%d now=%d deadline=%d\n",link,tfb_link_is_bus_available(link),link->tx_queue_len,tfb_time_now(),link->bus_available_deadline);

	//printf("link tick, tx_size=%d available=%d\n",link->tx_size,tfb_link_is_bus_available(link));

	if (tfb_link_is_bus_available(link) &&
			link->tx_size) {
		tfb_link_write_frame(link,link->tx_buf,link->tx_size);
		link->tx_size=0;
	}

	//printf("link tick done!!\n");
	//printf("link tick, available after: %zu\n",link->physical->available(link->physical));
}

void tfb_link_notify_bus_activity(tfb_link_t *link) {
	int delay=TFB_CA_DELAY_MIN+(rand()%(TFB_CA_DELAY_MAX-TFB_CA_DELAY_MIN));
	link->bus_available_deadline=tfb_time_future(link->physical,delay);
}

bool tfb_link_is_bus_available(tfb_link_t *link) {
	return tfb_time_expired(link->physical,link->bus_available_deadline);
}

bool tfb_link_send(tfb_link_t *link, uint8_t *data, size_t size, int flags) {
	//printf(">> sending link bytes: %zu... queue size: %d\n",size,link->tx_queue_len);

	if (compute_xor_checksum(data,size))
		return false;

	if (flags&TFB_LINK_URGENT)
		return tfb_link_write_frame(link,data,size);

	if (link->tx_size)
		return false;

	if (tfb_link_is_bus_available(link))
		return tfb_link_write_frame(link,data,size);

	memcpy(link->tx_buf,data,size);
	link->tx_size=size;

	return true;
}

tfb_time_t tfb_link_get_deadline(tfb_link_t *link) {
	if (!link->tx_size)
		return TFB_TIME_NEVER;

	return link->bus_available_deadline;
}

size_t tfb_link_peek_size(tfb_link_t *link) {
	if (link->rx_state==TFB_LINK_RX_COMPLETE)
		return link->rx_size;

	return 0;
}

uint8_t *tfb_link_peek(tfb_link_t *link) {
	if (link->rx_state==TFB_LINK_RX_COMPLETE)
		return link->rx_buf;

	return NULL;
}

void tfb_link_consume(tfb_link_t *link) {
	if (link->rx_state==TFB_LINK_RX_COMPLETE) {
		link->rx_state=TFB_LINK_RX_INIT;
		link->rx_size=0;
	}
}