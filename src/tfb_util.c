#include "tfb_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int tfb_allocated_blocks=0;

void *tfb_malloc(size_t size) {
	tfb_allocated_blocks++;
	return malloc(size);
}

char *tfb_strdup(char *s) {
    tfb_allocated_blocks++;
    return strdup(s);
}

void tfb_free(void *p) {
	tfb_allocated_blocks--;
	free(p);
}

/*void pointer_array_remove(void **array, size_t *size, size_t index) {
    if (!array || !size) return;          // sanity check
    if (index >= *size) return;           // out-of-bounds check

    // shift elements after index left by one
    for (size_t i = index; i + 1 < *size; ++i) {
        array[i] = array[i + 1];
    }

    (*size)--; // decrease size
}*/

uint8_t compute_xor_checksum(uint8_t *data, size_t size) {
    uint8_t check=0;
    for (int i=0; i<size; i++)
        check^=data[i];

    return check;
}
