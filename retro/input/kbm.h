#pragma once

#include <stdint.h>

void kbm_init(void);

void kbm_begin_frame(void);

int kbm_handles(unsigned device);

int16_t kbm_state(unsigned port, unsigned device, unsigned index, unsigned id);
