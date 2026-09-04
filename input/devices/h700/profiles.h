#pragma once

#include <linux/input.h>

#include "../../common/uinput.h"
#include "../device_rumble.h"

static const unsigned short h700_keys_stickless[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH, BTN_EAST,   BTN_NORTH, BTN_WEST, BTN_TL,
    BTN_TR,         BTN_TL2,      BTN_TR2,   BTN_SELECT, BTN_START, BTN_MODE,
};

static const unsigned short h700_keys_sticks[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH,  BTN_EAST,  BTN_NORTH, BTN_WEST,   BTN_TL,     BTN_TR,
    BTN_TL2,        BTN_TR2,      BTN_SELECT, BTN_START, BTN_MODE,  BTN_THUMBL, BTN_THUMBR,
};

static const struct gamepad_abs_desc h700_axes_stickless[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},
    {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},
    {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

static const struct gamepad_abs_desc h700_axes_sticks[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},  {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_RX, .min = -32767, .max = 32767}, {.code = ABS_RY, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},      {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

#define H700_DESC(_product, _keys, _axes)                                                                              \
    {                                                                                                                  \
        .name = MUOS_GAMEPAD_NAME,                                                                                     \
        .id = {BUS_VIRTUAL, muos_input_vendor, (_product), muos_input_version},                                        \
        .keys = (_keys),                                                                                               \
        .key_count = sizeof(_keys) / sizeof((_keys)[0]),                                                               \
        .axes = (_axes),                                                                                               \
        .axis_count = sizeof(_axes) / sizeof((_axes)[0]),                                                              \
        .ff_effects_max = device_rumble_effect_slots,                                                                  \
        .enable_ff_rumble = 1,                                                                                         \
    }

static const struct gamepad_desc rgsp_gamepad = H700_DESC(muos_product_rgsp, h700_keys_stickless, h700_axes_stickless);
static const struct gamepad_desc rg28xx_h_gamepad =
    H700_DESC(muos_product_rg28xx_h, h700_keys_stickless, h700_axes_stickless);
static const struct gamepad_desc rg34xx_h_gamepad =
    H700_DESC(muos_product_rg34xx_h, h700_keys_stickless, h700_axes_stickless);
static const struct gamepad_desc rg35xx_2024_gamepad =
    H700_DESC(muos_product_rg35xx_2024, h700_keys_stickless, h700_axes_stickless);
static const struct gamepad_desc rg35xx_plus_gamepad =
    H700_DESC(muos_product_rg35xx_plus, h700_keys_stickless, h700_axes_stickless);
static const struct gamepad_desc rg35xx_sp_gamepad =
    H700_DESC(muos_product_rg35xx_sp, h700_keys_stickless, h700_axes_stickless);

static const struct gamepad_desc rg34xx_sp_gamepad =
    H700_DESC(muos_product_rg34xx_sp, h700_keys_sticks, h700_axes_sticks);
static const struct gamepad_desc rg35xx_h_gamepad =
    H700_DESC(muos_product_rg35xx_h, h700_keys_sticks, h700_axes_sticks);
static const struct gamepad_desc rg35xx_pro_gamepad =
    H700_DESC(muos_product_rg35xx_pro, h700_keys_sticks, h700_axes_sticks);
static const struct gamepad_desc rg40xx_h_gamepad =
    H700_DESC(muos_product_rg40xx_h, h700_keys_sticks, h700_axes_sticks);
static const struct gamepad_desc rg40xx_v_gamepad =
    H700_DESC(muos_product_rg40xx_v, h700_keys_sticks, h700_axes_sticks);
static const struct gamepad_desc rgcubexx_h_gamepad =
    H700_DESC(muos_product_rgcubexx_h, h700_keys_sticks, h700_axes_sticks);

#undef H700_DESC
