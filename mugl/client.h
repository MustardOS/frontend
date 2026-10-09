#pragma once

#include <stdint.h>
#include "proto.h"

typedef struct {
    const char *name;
    void (*func)(void);
} mugl_proc;

extern const mugl_proc mugl_gl_procs[];
extern const unsigned mugl_gl_proc_count;

int mugl_connect(void);
int mugl_connected(void);
uint32_t mugl_width(void);
uint32_t mugl_height(void);

void mugl_lock(void);
void mugl_unlock(void);

void mugl_msg_begin(uint32_t op, uint32_t len);
void mugl_msg_put(const void *data, uint32_t len);
void mugl_msg_end(void);
const uint8_t *mugl_msg_wait(uint32_t *len);

void mugl_send(uint32_t op, const uint32_t *args, uint32_t count);
uint32_t mugl_call_u32(uint32_t op, const uint32_t *args, uint32_t count);

void mugl_swap(void);
void mugl_set_error(uint32_t error);
uint32_t mugl_take_error(void);

void (*mugl_gl_lookup(const char *name))(void);
void mugl_gl_reset(void);
