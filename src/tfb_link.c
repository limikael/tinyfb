#include "tfb_link.h"
#include "tfb_physical.h"
#include "tfb_util.h"
#include "tfb_frame.h"
#include <stdio.h>
#include <string.h>

tfb_link_t *tfb_link_create(tfb_physical_t *physical) {
	tfb_link_t *link=tfb_malloc(sizeof(tfb_link_t));

	link->physical=physical;
	link->rx_frame=tfb_frame_create(1024);
	link->tx_frame=NULL;
	link->rx_state=TFB_LINK_RX_INIT;
	link->tx_frame_owned=false;
	link->num_sent=0;
	link->busy=false;

	tfb_link_notify_bus_activity(link);

	tfb_physical_write_enable(link->physical,false);

	return link;
}

void tfb_link_set_busy(tfb_link_t *link, bool busy) {
	link->busy=busy;
	tfb_link_notify_bus_activity(link);
}

void tfb_link_dispose(tfb_link_t *link) {
	if (link->tx_frame_owned)
		tfb_frame_dispose(link->tx_frame);

	tfb_frame_dispose(link->rx_frame);
	tfb_free(link);
}

bool tfb_link_write_data(tfb_link_t *link, uint8_t *data, size_t size) {
	tfb_physical_write_enable(link->physical,true);
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
	tfb_physical_write_enable(link->physical,false);

	tfb_link_notify_bus_activity(link);

	return true;
}

tfb_frame_t *tfb_link_peek(tfb_link_t *link) {
	if (link->rx_state==TFB_LINK_RX_COMPLETE)
		return link->rx_frame;

	return NULL;
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
			if (tfb_frame_get_size(link->rx_frame) &&
					tfb_frame_is_valid(link->rx_frame)) {
				link->rx_state=TFB_LINK_RX_COMPLETE;
				return;
			}

			link->rx_state=TFB_LINK_RX_RECEIVE;
			tfb_frame_reset(link->rx_frame);
			break;

		case 0x7d:
			link->rx_state=TFB_LINK_RX_ESCAPE;
			break;

		default:
			if (link->rx_state==TFB_LINK_RX_ESCAPE)
				tfb_frame_write_byte(link->rx_frame,byte^0x20);

			else
				tfb_frame_write_byte(link->rx_frame,byte);

			link->rx_state=TFB_LINK_RX_RECEIVE;
			break;
	}
}

void tfb_link_drain(tfb_link_t *link) {
	if (!link->tx_frame || !tfb_link_is_bus_available(link))
		return;

	tfb_link_write_data(link,link->tx_frame->buffer,link->tx_frame->size);
	if (link->tx_frame_owned)
		tfb_frame_dispose(link->tx_frame);

	link->tx_frame=NULL;
	link->tx_frame_owned=false;
}

void tfb_link_tick(tfb_link_t *link) {
	int available=tfb_physical_available(link->physical);
	for (int i=0; i<available; i++) 
		tfb_link_handle_rx_byte(link,tfb_physical_read(link->physical));

	tfb_link_drain(link);
}

void tfb_link_notify_bus_activity(tfb_link_t *link) {
	int delay=TFB_CA_DELAY_MIN+(rand()%(TFB_CA_DELAY_MAX-TFB_CA_DELAY_MIN));
	link->bus_available_deadline=tfb_time_future(link->physical,delay);
}

bool tfb_link_is_bus_available(tfb_link_t *link) {
	if (link->busy)
		return false;

	return tfb_time_expired(link->physical,link->bus_available_deadline);
}

bool tfb_link_send(tfb_link_t *link, tfb_frame_t *frame, int flags) {
	if (!tfb_frame_has_data(frame,TFB_CHECKSUM))
		tfb_frame_write_checksum(frame);

	if (flags&TFB_LINK_SEND_URGENT) {
		tfb_link_write_data(link,frame->buffer,frame->size);
		if (flags&TFB_LINK_SEND_OWNED)
			tfb_frame_dispose(frame);

		return true;
	}

	if (link->tx_frame)
		return false;

	link->num_sent++;
	link->tx_frame=frame;
	link->tx_frame_owned=(flags&TFB_LINK_SEND_OWNED);

	tfb_link_drain(link);
	return true;
}

void tfb_link_unsend(tfb_link_t *link, tfb_frame_t *frame) {
	if (link->tx_frame!=frame)
		return;

	if (link->tx_frame_owned)
		tfb_frame_dispose(frame);

	link->tx_frame=NULL;
	link->tx_frame_owned=false;
}

tfb_time_t tfb_link_get_deadline(tfb_link_t *link) {
	if (!link->tx_frame || link->busy)
		return TFB_TIME_NEVER;

	return link->bus_available_deadline;
}

void tfb_link_consume(tfb_link_t *link) {
	if (link->rx_state==TFB_LINK_RX_COMPLETE) {
		link->rx_state=TFB_LINK_RX_INIT;
		tfb_frame_reset(link->rx_frame);
	}
}

uint32_t tfb_link_get_num_sent(tfb_link_t *link) {
	return link->num_sent;
}

uint32_t tfb_link_get_num_transmitted(tfb_link_t *link) {
	if (link->tx_frame)
		return link->num_sent-1;

	return link->num_sent;
}

bool tfb_link_is_transmitted(tfb_link_t *link, uint32_t sendnum) {
	return (int32_t)(tfb_link_get_num_transmitted(link)-sendnum)>=0;
}
