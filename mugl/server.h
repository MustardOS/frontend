#pragma once

#include <stdint.h>

float server_f(uint32_t value);
void server_reply(const void *data, uint32_t len);
void server_reply_u32(uint32_t value);
int server_missing(const char *name);
int server_load_gl(void);
int server_dispatch_generated(uint32_t op, const uint32_t *a, uint32_t words);
void server_read_frame(uint8_t *dst, int width, int height);
void server_frame_open(uint32_t width, uint32_t height);
void server_frame_publish(void);
void server_frame_close(void);
void server_frame_forget(void);
unsigned long server_client_ticks(void);
int server_overlay_active(void);
void server_overlay_draw(uint32_t width, uint32_t height, uint64_t work_ns);
