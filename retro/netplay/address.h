#pragma once

#include <stddef.h>
#include <stdint.h>

#define NETPLAY_DEFAULT_PORT 55435

int netplay_parse_address(const char *specification, char *address, size_t address_size, uint16_t *port);
