#include <string.h>
#include "wire.h"

uint16_t netplay_wire_read_u16(const uint8_t *data) {
    return (uint16_t) data[0] << 8 | data[1];
}

uint32_t netplay_wire_read_u32(const uint8_t *data) {
    return (uint32_t) data[0] << 24 | (uint32_t) data[1] << 16 | (uint32_t) data[2] << 8 | data[3];
}

uint64_t netplay_wire_read_u64(const uint8_t *data) {
    return (uint64_t) netplay_wire_read_u32(data) << 32 | netplay_wire_read_u32(data + 4);
}

void netplay_wire_write_u16(uint8_t *data, const uint16_t value) {
    data[0] = (uint8_t) (value >> 8);
    data[1] = (uint8_t) value;
}

void netplay_wire_write_u32(uint8_t *data, const uint32_t value) {
    data[0] = (uint8_t) (value >> 24);
    data[1] = (uint8_t) (value >> 16);
    data[2] = (uint8_t) (value >> 8);
    data[3] = (uint8_t) value;
}

void netplay_wire_write_u64(uint8_t *data, const uint64_t value) {
    netplay_wire_write_u32(data, (uint32_t) (value >> 32));
    netplay_wire_write_u32(data + 4, (uint32_t) value);
}

int netplay_wire_header_encode(uint8_t *data, const size_t size, const netplay_wire_header *header) {
    if (!data || !header || size < NETPLAY_WIRE_HEADER_SIZE || header->type == 0) return -1;
    memset(data, 0, NETPLAY_WIRE_HEADER_SIZE);
    memcpy(data, "PKNP", 4);
    data[4] = NETPLAY_WIRE_PROTOCOL;
    data[5] = header->type;
    netplay_wire_write_u32(data + 8, header->payload_size);
    netplay_wire_write_u32(data + 12, header->sequence);
    netplay_wire_write_u64(data + 16, header->frame);
    netplay_wire_write_u64(data + 24, header->session);
    return 0;
}

int netplay_wire_header_decode(const uint8_t *data, const size_t size, netplay_wire_header *header) {
    if (!data || !header || size != NETPLAY_WIRE_HEADER_SIZE || memcmp(data, "PKNP", 4) != 0
        || data[4] != NETPLAY_WIRE_PROTOCOL || data[5] == 0 || data[6] != 0 || data[7] != 0)
        return -1;
    header->type = data[5];
    header->payload_size = netplay_wire_read_u32(data + 8);
    header->sequence = netplay_wire_read_u32(data + 12);
    header->frame = netplay_wire_read_u64(data + 16);
    header->session = netplay_wire_read_u64(data + 24);
    return 0;
}

void netplay_wire_input_encode(uint8_t data[12], const netplay_pad_state *input, const uint8_t owner) {
    data[0] = owner;
    data[1] = input->connected;
    netplay_wire_write_u16(data + 2, input->buttons);
    for (unsigned index = 0; index < 4; index++)
        netplay_wire_write_u16(data + 4 + index * 2, (uint16_t) input->axes[index]);
}

int netplay_wire_input_decode(
    const uint8_t *data, const size_t size, netplay_pad_state *input, uint8_t *owner
) {
    if (!data || size != 12 || !input || !owner || data[1] > 1) return -1;
    *owner = data[0];
    input->connected = data[1];
    input->buttons = netplay_wire_read_u16(data + 2);
    for (unsigned index = 0; index < 4; index++)
        input->axes[index] = (int16_t) netplay_wire_read_u16(data + 4 + index * 2);
    return 0;
}
