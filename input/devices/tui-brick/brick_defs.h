#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../common/uinput.h"
#include "brick_structs.h"
#include "../device_rumble.h"

#define BRICK_AXIS_DESC(_code, _min, _max)                                                                             \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct brick_button BRICK_BUTTON_DEFS[] = {
    {-1, BTN_0, 0},       {-1, BTN_1, 0},       {116, KEY_VOLUMEDOWN, 0}, {117, KEY_VOLUMEUP, 0}, {109, BTN_SOUTH, 0},
    {108, BTN_EAST, 0},   {115, BTN_WEST, 0},   {114, BTN_NORTH, 0},      {36, BTN_TL, 0},        {241, BTN_TR, 0},
    {34, BTN_TL2, 0},     {37, BTN_TR2, 0},     {239, BTN_SELECT, 0},     {240, BTN_START, 0},    {238, BTN_MODE, 0},
    {236, BTN_THUMBL, 0}, {237, BTN_THUMBR, 0},
};

static const int BRICK_HAT_PINS[] = {113, 110, 111, 112};

static const unsigned short BRICK_KEYS[] = {
    BTN_0,  BTN_1,   KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH, BTN_EAST, BTN_WEST,   BTN_NORTH,  BTN_TL,
    BTN_TR, BTN_TL2, BTN_TR2,        BTN_SELECT,   BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR,
};

static const struct gamepad_abs_desc BRICK_AXES[] = {
    BRICK_AXIS_DESC(ABS_X, -32767, 32767),
    BRICK_AXIS_DESC(ABS_Y, -32767, 32767),
    BRICK_AXIS_DESC(ABS_HAT0X, -1, 1),
    BRICK_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

enum {
    BRICK_BUTTON_COUNT = sizeof(BRICK_BUTTON_DEFS) / sizeof(BRICK_BUTTON_DEFS[0]),
    BRICK_HAT_PIN_COUNT = sizeof(BRICK_HAT_PINS) / sizeof(BRICK_HAT_PINS[0]),
};

#define BRICK_DPAD_FLAG(path) MUINPUT_STATE_DIR "/" path

#define BRICK_BUTTON_BIT(idx) (1u << (idx))

enum {
    BRICK_BTN_IDX_F1 = 15,
    BRICK_BTN_IDX_F2 = 16,
    BRICK_BTN_IDX_L2 = 10,
    BRICK_BTN_IDX_R2 = 11,
    BRICK_BTN_IDX_SELECT = 12,
    BRICK_BTN_IDX_START = 13,
};

struct brick_hold_map {
    const char *path;
    uint32_t bit;
};

static const struct brick_hold_map BRICK_HOLD_MAP[] = {
    {BRICK_DPAD_FLAG("dpad2axis_hold_f1"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_F1)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_f2"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_F2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_l2"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_L2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_r2"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_R2)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_select"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_SELECT)},
    {BRICK_DPAD_FLAG("dpad2axis_hold_start"), BRICK_BUTTON_BIT(BRICK_BTN_IDX_START)},
};

enum { BRICK_HOLD_MAP_COUNT = sizeof(BRICK_HOLD_MAP) / sizeof(BRICK_HOLD_MAP[0]) };

enum { BRICK_GPIO_SWITCH = 243 };

static const unsigned short BRICK_SWITCHES[] = {SW_TABLET_MODE};

static const struct gamepad_desc BRICK_GAMEPAD_DESC = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = MUOS_INPUT_VENDOR,
            .product = MUOS_PRODUCT_TUI_BRICK,
            .version = MUOS_INPUT_VERSION,
        },
    .keys = BRICK_KEYS,
    .key_count = sizeof(BRICK_KEYS) / sizeof(BRICK_KEYS[0]),
    .axes = BRICK_AXES,
    .axis_count = sizeof(BRICK_AXES) / sizeof(BRICK_AXES[0]),
    .switches = BRICK_SWITCHES,
    .switch_count = sizeof(BRICK_SWITCHES) / sizeof(BRICK_SWITCHES[0]),
    .ff_effects_max = DEVICE_RUMBLE_EFFECT_SLOTS,
    .enable_ff_rumble = 1,
};
