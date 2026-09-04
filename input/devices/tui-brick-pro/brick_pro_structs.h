#pragma once

#include "../device_helpers.h"
#include "../device_rumble.h"
#include "../turbo.h"

struct gamepad;
struct axis_state;

struct brick_pro_button {
    int gpio;
    unsigned short code;
    int prev;
};

static const struct turbo_binding_cfg brick_pro_turbo_cfg[] = {
    {TURBO_FLAG_PREFIX "a", BTN_EAST}, {TURBO_FLAG_PREFIX "b", BTN_SOUTH}, {TURBO_FLAG_PREFIX "x", BTN_NORTH},
    {TURBO_FLAG_PREFIX "y", BTN_WEST}, {TURBO_FLAG_PREFIX "l", BTN_TL},    {TURBO_FLAG_PREFIX "l2", BTN_TL2},
    {TURBO_FLAG_PREFIX "r", BTN_TR},   {TURBO_FLAG_PREFIX "r2", BTN_TR2},
};

enum { brick_pro_turbo_cfg_count = sizeof(brick_pro_turbo_cfg) / sizeof(brick_pro_turbo_cfg[0]) };

struct brick_pro_state {
    struct gamepad *gp;
    struct brick_pro_button buttons[18];
    int hat_pins[4];
    struct device_hat_state hat;
    struct device_dirty_state dirty;
    int last_switch;
    int active_low;
    struct device_rumble_state rumble;
    struct turbo_binding turbo[8];
    size_t turbo_count;

    int i2c_fd;
    struct axis_state *ax_lx;
    struct axis_state *ax_ly;
    struct axis_state *ax_rx;
    struct axis_state *ax_ry;
    int last_lx;
    int last_ly;
    int last_rx;
    int last_ry;
};
