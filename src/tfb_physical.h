#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct tfb_physical tfb_physical_t;

struct tfb_physical {
	void *tag;

	size_t (*write)(tfb_physical_t *physical, uint8_t data);
	int (*read)(tfb_physical_t *physical);
	size_t (*available)(tfb_physical_t *physical);
	uint32_t (*millis)(tfb_physical_t *physical);
	void (*write_enable)(tfb_physical_t *physical, bool enable);
};

tfb_physical_t *tfb_physical_create();
void tfb_physical_dispose(tfb_physical_t *physical);
void tfb_physical_func(tfb_physical_t *physical, int id, void *func);
void tfb_physical_write(tfb_physical_t *physical, uint8_t data);
int tfb_physical_read(tfb_physical_t *physical);
size_t tfb_physical_available(tfb_physical_t *physical);
uint32_t tfb_physical_millis(tfb_physical_t *physical);
void tfb_physical_write_enable(tfb_physical_t *physical, bool enable);

#ifdef __cplusplus
}
#endif
