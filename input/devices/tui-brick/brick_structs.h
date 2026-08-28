#pragma once

#include "../device_helpers.h"
#include "../device_rumble.h"
#include "../turbo.h"

struct gamepad;

struct brick_button {
    int gpio;
    unsigned short code;
    int prev;
};

static const struct turbo_binding_cfg BRICK_TURBO_CFG[] = {
    {TURBO_FLAG_PREFIX "a", BTN_EAST}, {TURBO_FLAG_PREFIX "b", BTN_SOUTH}, {TURBO_FLAG_PREFIX "x", BTN_NORTH},
    {TURBO_FLAG_PREFIX "y", BTN_WEST}, {TURBO_FLAG_PREFIX "l", BTN_TL},    {TURBO_FLAG_PREFIX "l2", BTN_TL2},
    {TURBO_FLAG_PREFIX "r", BTN_TR},   {TURBO_FLAG_PREFIX "r2", BTN_TR2},
};

enum { BRICK_TURBO_CFG_COUNT = sizeof(BRICK_TURBO_CFG) / sizeof(BRICK_TURBO_CFG[0]) };

enum brick_dpad2axis_mode {
    BRICK_D2A_MODE_DISABLED = 0,
    BRICK_D2A_MODE_GLOBAL = 1,
    BRICK_D2A_MODE_HOLD = 2,
};

struct brick_state {
    struct gamepad *gp;
    struct brick_button buttons[17];
    int hat_pins[4];
    struct device_hat_state hat;
    struct device_dirty_state dirty;
    int last_switch;
    int active_low;
    struct device_rumble_state rumble;
    struct turbo_binding turbo[8];
    size_t turbo_count;
    int toggle_dpad;
    int toggle_dpad2axis;
    uint32_t alt_dpad2axis_bits;
    uint32_t state_buttons;
    enum brick_dpad2axis_mode d2a_last_mode;
    int last_axis_x;
    int last_axis_y;
};
