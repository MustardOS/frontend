#pragma once

#include <stdint.h>

typedef struct {
    uint32_t delay;
    uint32_t tick;
} nav_repeat_t;

int nav_repeat_step(nav_repeat_t *state, int edge, int held, int repeat_allowed, uint32_t now);

uint64_t nav_mask_standard(void);
