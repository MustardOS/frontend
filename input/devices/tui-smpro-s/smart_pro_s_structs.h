#pragma once

#include <stdint.h>
#include <poll.h>

#include "../device_helpers.h"
#include "../turbo.h"
#include "../../drivers/serial/serial.h"

struct gamepad;
struct axis_state;

struct sp_s_ctx_common {
    int fd;
    struct ring_buffer rb;
    struct gamepad *gp;
    struct axis_state *ax;
    struct axis_state *ay;
    uint32_t prev_buttons;
    int last_x;
    int last_y;
    uint16_t prev_raw_x;
    uint16_t prev_raw_y;
    int have_prev_raw;
};

struct smart_pro_s_left_ctx {
    struct sp_s_ctx_common c;
    int last_switch;
    struct device_hat_state hat;
};

struct smart_pro_s_right_ctx {
    struct sp_s_ctx_common c;
};

struct sp_s_button_map_entry {
    uint32_t mask;
    unsigned short code;
};

static const struct turbo_binding_cfg sp_s_turbo_cfg[] = {
    {TURBO_FLAG_PREFIX "a", BTN_EAST}, {TURBO_FLAG_PREFIX "b", BTN_SOUTH}, {TURBO_FLAG_PREFIX "x", BTN_NORTH},
    {TURBO_FLAG_PREFIX "y", BTN_WEST}, {TURBO_FLAG_PREFIX "l", BTN_TL},    {TURBO_FLAG_PREFIX "l2", BTN_TL2},
    {TURBO_FLAG_PREFIX "r", BTN_TR},   {TURBO_FLAG_PREFIX "r2", BTN_TR2},
};

enum { sp_s_turbo_cfg_count = sizeof(sp_s_turbo_cfg) / sizeof(sp_s_turbo_cfg[0]) };

#pragma pack(push, 1)

union sp_b_buttons {
    uint32_t raw;
    struct {
        unsigned a : 1;
        unsigned x : 1;
        unsigned select : 1;
        unsigned start : 1;
        unsigned dpad_up : 1;
        unsigned dpad_down : 1;
        unsigned dpad_left : 1;
        unsigned dpad_right : 1;
        unsigned b : 1;
        unsigned y : 1;
        unsigned l1 : 1;
        unsigned r1 : 1;
        unsigned l2 : 1;
        unsigned r2 : 1;
        unsigned l3 : 1;
        unsigned r3 : 1;
        unsigned mode : 1;
        unsigned reserved : 15;
    } bits;
};

struct sp_frame_b {
    uint8_t start;
    uint8_t unknown;
    union sp_b_buttons buttons;
    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
    uint8_t reserved[4];
    uint8_t end;
    uint8_t reserved2;
};
#pragma pack(pop)

struct smart_pro_s_device {
    struct smart_pro_s_left_ctx left;
    struct smart_pro_s_right_ctx right;
    struct device_dirty_state dirty;
    struct turbo_binding turbo[8];
    size_t turbo_count;
    struct pollfd pfds[2];
};
