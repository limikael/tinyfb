#include "tfb_physical.h"
#include "tfb_util.h"
#include <stdlib.h>
#include <stdio.h>

tfb_physical_t *tfb_physical_create() {
	tfb_physical_t *physical=tfb_malloc(sizeof(tfb_physical_t));

	physical->write=NULL;
	physical->read=NULL;
	physical->available=NULL;
	physical->millis=NULL;

	return physical;
}

void tfb_physical_dispose(tfb_physical_t *physical) {
	tfb_free(physical);
}

void tfb_physical_func(tfb_physical_t *physical, int id, void *func) {
	switch (id) {
		case 1: physical->write=func; break;
		case 2: physical->read=func; break;
		case 3: physical->available=func; break;
		case 4: physical->millis=func; break;

		default:
			printf("warning... unknown physical function id: %d\n",id);
			break;
	}
}

void tfb_physical_write(tfb_physical_t *physical, uint8_t data) {
	physical->write(physical,data);
}

size_t tfb_physical_available(tfb_physical_t *physical) {
	return physical->available(physical);
}

int tfb_physical_read(tfb_physical_t *physical) {
	return physical->read(physical);
}

uint32_t tfb_physical_millis(tfb_physical_t *physical) {
	return physical->millis(physical);
}
