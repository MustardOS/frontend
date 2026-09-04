#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../common/uinput.h"
#include "brick_structs.h"
#include "../device_rumble.h"

#define BRICK_AXIS_DESC(_code, _min, _max)                                                                             \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct brick_button brick_button_defs[] = {
    {-1, BTN_0, 0},       {-1, BTN_1, 0},       {116, KEY_VOLUMEDOWN, 0}, {117, KEY_VOLUMEUP, 0}, {109, BTN_SOUTH, 0},
    {108, BTN_EAST, 0},   {115, BTN_WEST, 0},   {114, BTN_NORTH, 0},      {36, BTN_TL, 0},        {241, BTN_TR, 0},
    {34, BTN_TL2, 0},     {37, BTN_TR2, 0},     {239, BTN_SELECT, 0},     {240, BTN_START, 0},    {238, BTN_MODE, 0},
    {236, BTN_THUMBL, 0}, {237, BTN_THUMBR, 0},
};

static const int brick_hat_pins[] = {113, 110, 111, 112};

static const unsigned short brick_keys[] = {
    BTN_0,  BTN_1,   KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH, BTN_EAST, BTN_WEST,   BTN_NORTH,  BTN_TL,
    BTN_TR, BTN_TL2, BTN_TR2,        BTN_SELECT,   BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR,
};

static const struct gamepad_abs_desc brick_axes[] = {
    BRICK_AXIS_DESC(ABS_X, -32767, 32767),
    BRICK_AXIS_DESC(ABS_Y, -32767, 32767),
    BRICK_AXIS_DESC(ABS_HAT0X, -1, 1),
    BRICK_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

enum {
    brick_button_count = sizeof(brick_button_defs) / sizeof(brick_button_defs[0]),
    brick_hat_pin_count = sizeof(brick_hat_pins) / sizeof(brick_hat_pins[0]),
};

#define BRICK_DPAD_FLAG(path) MUINPUT_STATE_DIR "/" path

#define BRICK_BUTTON_BIT(idx) (1u << (idx))

enum {
    brick_btn_idx_f1 = 15,
    brick_btn_idx_f2 = 16,
    brick_btn_idx_l2 = 10,
    brick_btn_idx_r2 = 11,
    brick_btn_idx_select = 12,
    brick_btn_idx_start = 13,
};

struct brick_hold_map {
    const char *path;
    uint32_t bit;
};

static const struct brick_hold_map brick_hold_map[] = {
    {BRICK_DPAD_FLAG("dpad2axis_hold_f1"), BRICK_BUTTON_BIT(brick_btn_idx_f1)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_f2"), BRICK_BUTTON_BIT(brick_btn_idx_f2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_l2"), BRICK_BUTTON_BIT(brick_btn_idx_l2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_r2"), BRICK_BUTTON_BIT(brick_btn_idx_r2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_select"), BRICK_BUTTON_BIT(brick_btn_idx_select)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_start"), BRICK_BUTTON_BIT(brick_btn_idx_start)},
};

enum { brick_hold_map_count = sizeof(brick_hold_map) / sizeof(brick_hold_map[0]) };

enum { brick_gpio_switch = 243 };

static const unsigned short brick_switches[] = {SW_TABLET_MODE};

static const struct gamepad_desc brick_gamepad_desc = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = muos_input_vendor,
            .product = muos_product_tui_brick,
            .version = muos_input_version,
        },
    .keys = brick_keys,
    .key_count = sizeof(brick_keys) / sizeof(brick_keys[0]),
    .axes = brick_axes,
    .axis_count = sizeof(brick_axes) / sizeof(brick_axes[0]),
    .switches = brick_switches,
    .switch_count = sizeof(brick_switches) / sizeof(brick_switches[0]),
    .ff_effects_max = device_rumble_effect_slots,
    .enable_ff_rumble = 1,
};
