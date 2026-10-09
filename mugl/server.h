#pragma once

#include <stdint.h>

float server_f(uint32_t value);
void server_reply(const void *data, uint32_t len);
void server_reply_u32(uint32_t value);
int server_missing(const char *name);
int server_load_gl(void);
int server_dispatch_generated(uint32_t op, const uint32_t *a, uint32_t words);
