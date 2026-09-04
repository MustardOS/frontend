#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../common/uinput.h"
#include "brick_pro_structs.h"
#include "../device_rumble.h"

#define BRICK_PRO_AXIS_DESC(_code, _min, _max)                                                                         \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct brick_pro_button brick_pro_button_defs[] = {
    {116, KEY_VOLUMEDOWN, 0}, {117, KEY_VOLUMEUP, 0}, {109, BTN_SOUTH, 0},    {108, BTN_EAST, 0},  {115, BTN_WEST, 0},
    {114, BTN_NORTH, 0},      {34, BTN_TL, 0},        {241, BTN_TR, 0},       {36, BTN_TL2, 0},    {37, BTN_TR2, 0},
    {135, BTN_THUMBL, 0},     {136, BTN_THUMBR, 0},   {237, BTN_SELECT, 0},   {238, BTN_START, 0}, {236, BTN_MODE, 0},
    {239, KEY_F1, 0},         {240, KEY_F2, 0},       {242, KEY_HOMEPAGE, 0},
};

static const int brick_pro_hat_pins[] = {113, 110, 111, 112};

static const unsigned short brick_pro_keys[] = {
    KEY_VOLUMEDOWN, KEY_VOLUMEUP, BTN_SOUTH,  BTN_EAST,   BTN_WEST,  BTN_NORTH, BTN_TL, BTN_TR, BTN_TL2,
    BTN_TR2,        BTN_THUMBL,   BTN_THUMBR, BTN_SELECT, BTN_START, BTN_MODE,  KEY_F1, KEY_F2, KEY_HOMEPAGE,
};

static const struct gamepad_abs_desc brick_pro_axes[] = {
    BRICK_PRO_AXIS_DESC(ABS_X, -32767, 32767),  BRICK_PRO_AXIS_DESC(ABS_Y, -32767, 32767),
    BRICK_PRO_AXIS_DESC(ABS_RX, -32767, 32767), BRICK_PRO_AXIS_DESC(ABS_RY, -32767, 32767),
    BRICK_PRO_AXIS_DESC(ABS_HAT0X, -1, 1),      BRICK_PRO_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

enum {
    brick_pro_button_count = sizeof(brick_pro_button_defs) / sizeof(brick_pro_button_defs[0]),
    brick_pro_hat_pin_count = sizeof(brick_pro_hat_pins) / sizeof(brick_pro_hat_pins[0]),
};

enum { brick_pro_gpio_switch = 243 };

static const unsigned short brick_pro_switches[] = {SW_TABLET_MODE};

#define BRICK_PRO_I2C_DEV      "/dev/i2c-3"
#define BRICK_PRO_I2C_ADDR_L   (0x29)
#define BRICK_PRO_I2C_ADDR_R   (0x28)
#define BRICK_PRO_I2C_REG_POLL (0xB0)
#define BRICK_PRO_AXIS_RAW_MAX (0xFFF)

static const struct gamepad_desc brick_pro_gamepad_desc = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = muos_input_vendor,
            .product = muos_product_tui_brick_pro,
            .version = muos_input_version,
        },
    .keys = brick_pro_keys,
    .key_count = sizeof(brick_pro_keys) / sizeof(brick_pro_keys[0]),
    .axes = brick_pro_axes,
    .axis_count = sizeof(brick_pro_axes) / sizeof(brick_pro_axes[0]),
    .switches = brick_pro_switches,
    .switch_count = sizeof(brick_pro_switches) / sizeof(brick_pro_switches[0]),
    .ff_effects_max = device_rumble_effect_slots,
    .enable_ff_rumble = 1,
};
