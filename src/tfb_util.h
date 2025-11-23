#pragma once
#include <stdlib.h>
#include <stdint.h>

//void pointer_array_remove(void **array, size_t *size, size_t index);
uint8_t compute_xor_checksum(uint8_t *data, size_t size);
void *tfb_malloc(size_t size);
char *tfb_strdup(char *s);
void tfb_free(void *p);

extern int tfb_allocated_blocks;
