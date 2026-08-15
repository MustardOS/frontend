#pragma once

#include <stdint.h>

#define STATE_HEADER_SIZE  32U
#define STATE_VERSION      1U
#define STATE_FLAG_CHEEVO  1U
#define STATE_CHEEVO_LIMIT (16U * 1024U * 1024U)

static inline void state_write_u16(uint8_t *data, const uint16_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
}

static inline void state_write_u32(uint8_t *data, const uint32_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}

static inline uint16_t state_read_u16(const uint8_t *data) {
    return (uint16_t) data[0] | (uint16_t) data[1] << 8;
}

static inline uint32_t state_read_u32(const uint8_t *data) {
    return (uint32_t) data[0] | (uint32_t) data[1] << 8 | (uint32_t) data[2] << 16 | (uint32_t) data[3] << 24;
}
