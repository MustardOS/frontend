#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define RB_CAPACITY (sizeof(((struct ring_buffer *) 0)->data))
#define RB_MASK     (RB_CAPACITY - 1)

_Static_assert((RB_CAPACITY & RB_MASK) == 0, "RB_CAPACITY must be power of two");

static size_t rb_index(const struct ring_buffer *rb, size_t offset) {
    return (rb->head + offset) & RB_MASK;
}

void rb_initialise(struct ring_buffer *rb) {
    rb->head = 0;
    rb->len = 0;
}

static void rb_drop(struct ring_buffer *rb, size_t count) {
    if (count > rb->len) {
        count = rb->len;
    }
    rb->head = (rb->head + count) & RB_MASK;
    rb->len -= count;
}

void rb_push(struct ring_buffer *rb, const uint8_t *data, size_t len) {
    if (len >= RB_CAPACITY) {
        data += (len - RB_CAPACITY);
        len = RB_CAPACITY;
        rb->head = 0;
        rb->len = 0;
    }
    while (rb->len + len > RB_CAPACITY) {
        rb_drop(rb, (rb->len + len) - RB_CAPACITY);
    }

    size_t tail = rb_index(rb, rb->len);
    size_t first = RB_CAPACITY - tail;
    if (first > len) {
        first = len;
    }
    memcpy(&rb->data[tail], data, first);
    if (len > first) {
        memcpy(&rb->data[0], data + first, len - first);
    }
    rb->len += len;
}

ssize_t rb_find(struct ring_buffer *rb, uint8_t start_byte) {
    if (rb->len == 0) {
        return -1;
    }

    size_t head = rb->head;
    size_t first_chunk = RB_CAPACITY - head;
    if (first_chunk > rb->len) {
        first_chunk = rb->len;
    }

    const uint8_t *p = memchr(&rb->data[head], start_byte, first_chunk);
    if (p) {
        return (ssize_t) (p - &rb->data[head]);
    }

    size_t remaining = rb->len - first_chunk;
    if (remaining == 0) {
        return -1;
    }

    const uint8_t *q = memchr(&rb->data[0], start_byte, remaining);
    return q ? (ssize_t) (first_chunk + (size_t) (q - &rb->data[0])) : -1;
}

static void rb_copy_out(const struct ring_buffer *rb, size_t offset, uint8_t *out, size_t len) {
    size_t start = rb_index(rb, offset);
    size_t first = RB_CAPACITY - start;
    if (first > len) {
        first = len;
    }
    memcpy(out, &rb->data[start], first);
    if (len > first) {
        memcpy(out + first, &rb->data[0], len - first);
    }
}

static inline uint8_t rb_peek(const struct ring_buffer *rb, size_t offset) {
    return rb->data[rb_index(rb, offset)];
}

int rb_try_extract_frame_variantA(struct ring_buffer *rb, uint8_t *out8) {
    for (;;) {
        ssize_t idx = rb_find(rb, 0xFF);
        if (idx < 0) {
            return 0;
        }
        size_t start = (size_t) idx;
        if (rb->len - start < 8) {
            return 0;
        }
        if (rb_peek(rb, start + 7) == 0xFE) {
            rb_copy_out(rb, start, out8, 8);
            rb_drop(rb, start + 8);
            return 1;
        }
        rb_drop(rb, start + 1);
    }
}

int rb_try_extract_frame_variantB(struct ring_buffer *rb, uint8_t *out20) {
    for (;;) {
        ssize_t idx = rb_find(rb, 0xFF);
        if (idx < 0) {
            return 0;
        }
        size_t start = (size_t) idx;
        if (rb->len - start < 20) {
            return 0;
        }
        if (rb_peek(rb, start + 0x12) == 0xFE) {
            rb_copy_out(rb, start, out20, 20);
            rb_drop(rb, start + 20);
            return 1;
        }
        rb_drop(rb, start + 1);
    }
}

int serial_open_nonblocking(const char *path) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("serial open");
    }
    return fd;
}

int serial_config(int fd) {
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        perror("tcgetattr");
        return -1;
    }

    cfmakeraw(&t);
    cfsetispeed(&t, B19200);
    cfsetospeed(&t, B19200);
    t.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
    t.c_cflag &= ~CRTSCTS;
#endif
    t.c_cflag &= ~CSTOPB;
    t.c_cflag &= ~PARENB;
    t.c_cflag &= ~CSIZE;
    t.c_cflag |= CS8;

    if (tcsetattr(fd, TCSANOW, &t) < 0) {
        perror("tcsetattr");
        return -1;
    }
    return 0;
}

ssize_t serial_poll_read(int fd, uint8_t *buf, size_t max) {
    ssize_t total = 0;
    for (;;) {
        ssize_t r = read(fd, buf + total, max - (size_t) total);
        if (r > 0) {
            total += r;
            if ((size_t) total == max) {
                return total;
            }
            continue;
        }
        if (r == 0) {
            return total;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return total;
        }
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
}
