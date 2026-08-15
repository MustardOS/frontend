#pragma once

#include <stddef.h>
#include <stdint.h>

struct core_state_buffer {
    uint8_t *data;
    size_t capacity;
    size_t size;
};

void core_state_session_init(void);

int core_state_available(void);

size_t core_state_reported_size(size_t caller_limit, const char *operation);

int core_state_capture(
    struct core_state_buffer *buffer, size_t requested_size, size_t caller_limit, int verify_after,
    const char *operation
);

int core_state_capture_prefixed(
    struct core_state_buffer *buffer, size_t prefix_size, size_t requested_size, size_t caller_limit,
    int verify_after, const char *operation
);

int core_state_restore(const void *data, size_t size, size_t caller_limit, const char *operation);

void core_state_buffer_release(struct core_state_buffer *buffer);
