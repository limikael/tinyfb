#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tfb_time.h"

#define TFB_CHECKSUM 1
#define TFB_FROM 2
#define TFB_TO 3
#define TFB_PAYLOAD 4
#define TFB_SEQ 5
#define TFB_ACK 6
#define TFB_ANNOUNCE_NAME 7
#define TFB_ASSIGN_NAME 8
#define TFB_SESSION_ID 9
#define TFB_RESET_TO 10

typedef struct tfb_frame tfb_frame_t;

struct tfb_frame {
	uint8_t *buffer;
	size_t size, capacity;
	int resend_count;
	tfb_time_t deadline;
	uint32_t submission_number;
};

tfb_frame_t *tfb_frame_create(size_t size);
tfb_frame_t *tfb_frame_create_from_data(uint8_t *data, size_t size);
void tfb_frame_dispose(tfb_frame_t *frame);
uint8_t tfb_frame_get_buffer_at(tfb_frame_t *frame, size_t index);
uint8_t *tfb_frame_get_buffer(tfb_frame_t *frame);
size_t tfb_frame_get_size(tfb_frame_t *frame);
size_t tfb_frame_get_data_size(tfb_frame_t *frame, uint8_t key);
uint8_t *tfb_frame_get_data(tfb_frame_t *frame, uint8_t key);
int tfb_frame_strcmp(tfb_frame_t *frame, uint8_t key, char *s);
char *tfb_frame_get_strdup(tfb_frame_t *frame, uint8_t key);
int tfb_frame_get_num(tfb_frame_t *frame, uint8_t key);
uint8_t tfb_frame_get_checksum(tfb_frame_t *frame);
bool tfb_frame_has_data(tfb_frame_t *frame, uint8_t key);
void tfb_frame_write_byte(tfb_frame_t *frame, uint8_t byte);
void tfb_frame_write_data(tfb_frame_t *frame, uint8_t key, uint8_t *bytes, size_t size);
void tfb_frame_write_num(tfb_frame_t *frame, uint8_t key, int num);
void tfb_frame_write_checksum(tfb_frame_t *frame);
char *tfb_frame_sprint(tfb_frame_t *frame, char *s);
void tfb_frame_reset(tfb_frame_t *frame);
