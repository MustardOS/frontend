#pragma once

#include <linux/input.h>

#include "../../common/uinput.h"
#include "../device_rumble.h"

static const unsigned short H700_KEYS_STICKLESS[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH, BTN_EAST,   BTN_NORTH, BTN_WEST, BTN_TL,
    BTN_TR,         BTN_TL2,      BTN_TR2,   BTN_SELECT, BTN_START, BTN_MODE,
};

static const unsigned short H700_KEYS_STICKS[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH,  BTN_EAST,  BTN_NORTH, BTN_WEST,   BTN_TL,     BTN_TR,
    BTN_TL2,        BTN_TR2,      BTN_SELECT, BTN_START, BTN_MODE,  BTN_THUMBL, BTN_THUMBR,
};

static const struct gamepad_abs_desc H700_AXES_STICKLESS[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},
    {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},
    {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

static const struct gamepad_abs_desc H700_AXES_STICKS[] = {
    {.code = ABS_X, .min = -32767, .max = 32767},  {.code = ABS_Y, .min = -32767, .max = 32767},
    {.code = ABS_RX, .min = -32767, .max = 32767}, {.code = ABS_RY, .min = -32767, .max = 32767},
    {.code = ABS_HAT0X, .min = -1, .max = 1},      {.code = ABS_HAT0Y, .min = -1, .max = 1},
};

#define H700_DESC(_product, _keys, _axes)                                                                              \
    {                                                                                                                  \
        .name = MUOS_GAMEPAD_NAME,                                                                                     \
        .id = {BUS_VIRTUAL, MUOS_INPUT_VENDOR, (_product), MUOS_INPUT_VERSION},                                        \
        .keys = (_keys),                                                                                               \
        .key_count = sizeof(_keys) / sizeof((_keys)[0]),                                                               \
        .axes = (_axes),                                                                                               \
        .axis_count = sizeof(_axes) / sizeof((_axes)[0]),                                                              \
        .ff_effects_max = DEVICE_RUMBLE_EFFECT_SLOTS,                                                                  \
        .enable_ff_rumble = 1,                                                                                         \
    }

static const struct gamepad_desc RGSP_GAMEPAD = H700_DESC(MUOS_PRODUCT_RGSP, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);
static const struct gamepad_desc RG28XX_H_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG28XX_H, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);
static const struct gamepad_desc RG34XX_H_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG34XX_H, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);
static const struct gamepad_desc RG35XX_2024_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG35XX_2024, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);
static const struct gamepad_desc RG35XX_PLUS_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG35XX_PLUS, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);
static const struct gamepad_desc RG35XX_SP_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG35XX_SP, H700_KEYS_STICKLESS, H700_AXES_STICKLESS);

static const struct gamepad_desc RG34XX_SP_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG34XX_SP, H700_KEYS_STICKS, H700_AXES_STICKS);
static const struct gamepad_desc RG35XX_H_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG35XX_H, H700_KEYS_STICKS, H700_AXES_STICKS);
static const struct gamepad_desc RG35XX_PRO_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG35XX_PRO, H700_KEYS_STICKS, H700_AXES_STICKS);
static const struct gamepad_desc RG40XX_H_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG40XX_H, H700_KEYS_STICKS, H700_AXES_STICKS);
static const struct gamepad_desc RG40XX_V_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RG40XX_V, H700_KEYS_STICKS, H700_AXES_STICKS);
static const struct gamepad_desc RGCUBEXX_H_GAMEPAD =
    H700_DESC(MUOS_PRODUCT_RGCUBEXX_H, H700_KEYS_STICKS, H700_AXES_STICKS);

#undef H700_DESC
