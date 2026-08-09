#pragma once

#include <stddef.h>
#include <stdint.h>
#include "netplay.h"

#define NETPLAY_WIRE_PROTOCOL    4U
#define NETPLAY_WIRE_HEADER_SIZE 32U

typedef struct {
    uint8_t type;
    uint32_t payload_size;
    uint32_t sequence;
    uint64_t frame;
    uint64_t session;
} netplay_wire_header;

uint16_t netplay_wire_read_u16(const uint8_t *data);
uint32_t netplay_wire_read_u32(const uint8_t *data);
uint64_t netplay_wire_read_u64(const uint8_t *data);
void netplay_wire_write_u16(uint8_t *data, uint16_t value);
void netplay_wire_write_u32(uint8_t *data, uint32_t value);
void netplay_wire_write_u64(uint8_t *data, uint64_t value);
int netplay_wire_header_encode(uint8_t *data, size_t size, const netplay_wire_header *header);
int netplay_wire_header_decode(const uint8_t *data, size_t size, netplay_wire_header *header);
void netplay_wire_input_encode(uint8_t data[12], const netplay_pad_state *input, uint8_t owner);
int netplay_wire_input_decode(const uint8_t *data, size_t size, netplay_pad_state *input, uint8_t *owner);
