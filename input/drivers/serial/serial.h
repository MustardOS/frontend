#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct ring_buffer {
    uint8_t data[2048];
    size_t head;
    size_t len;
};

int serial_open_nonblocking(const char *path);

int serial_config(int fd);

ssize_t serial_poll_read(int fd, uint8_t *buf, size_t max);

void rb_initialise(struct ring_buffer *rb);

void rb_push(struct ring_buffer *rb, const uint8_t *data, size_t len);

ssize_t rb_find(struct ring_buffer *rb, uint8_t start_byte);

int rb_try_extract_frame_variantA(struct ring_buffer *rb, uint8_t *out8);

int rb_try_extract_frame_variantB(struct ring_buffer *rb, uint8_t *out20);
