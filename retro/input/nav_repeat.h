#pragma once

#include <stdint.h>
#include "../../common/input.h"

#define NAV_PAGE_UP_BIT   BIT(12)
#define NAV_PAGE_DOWN_BIT BIT(13)

typedef struct {
    uint32_t delay;
    uint32_t tick;
} nav_repeat_t;

int nav_repeat_step(nav_repeat_t *state, int edge, int held, int repeat_allowed, uint32_t now);

uint64_t nav_mask_standard(void);

uint64_t nav_mask_page(void);

int nav_input_halted(void);

int nav_page_tick(uint64_t edge, uint64_t mask, int long_dot);
