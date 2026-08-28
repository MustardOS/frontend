#pragma once

#include <stddef.h>

#include "../../common/uinput.h"
#include "smart_pro_structs.h"
#include "../device_rumble.h"

enum {
    SP_BTN_L1_MASK = 0x01,
    SP_BTN_L2_MASK = 0x02,
    SP_BTN_DPAD_UP_MASK = 0x04,
    SP_BTN_DPAD_LEFT_MASK = 0x08,
    SP_BTN_DPAD_RIGHT_MASK = 0x10,
    SP_BTN_DPAD_DOWN_MASK = 0x20,
    SP_BTN_MODE_MASK = 0x80,
};

enum {
    SP_BTN_R1_MASK = 0x01,
    SP_BTN_R2_MASK = 0x02,
    SP_BTN_Y_MASK = 0x04,
    SP_BTN_X_MASK = 0x08,
    SP_BTN_B_MASK = 0x10,
    SP_BTN_A_MASK = 0x20,
    SP_BTN_SELECT_MASK = 0x40,
    SP_BTN_START_MASK = 0x80,
};

enum { SP_DPAD_MASK = SP_BTN_DPAD_UP_MASK | SP_BTN_DPAD_DOWN_MASK | SP_BTN_DPAD_LEFT_MASK | SP_BTN_DPAD_RIGHT_MASK };

enum {
    SP_GPIO_OUT1 = 110,
    SP_GPIO_OUT2 = 114,
    SP_GPIO_INPUT = 243,
};

#define SP_AXIS_DESC(_code, _min, _max)                                                                                \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct sp_button_map_entry SP_LEFT_BUTTON_MAP[] = {
    {SP_BTN_L1_MASK, BTN_TL},
    {SP_BTN_L2_MASK, BTN_TL2},
    {SP_BTN_MODE_MASK, BTN_MODE},
};

static const struct sp_button_map_entry SP_RIGHT_BUTTON_MAP[] = {
    {SP_BTN_R1_MASK, BTN_TR},         {SP_BTN_R2_MASK, BTN_TR2},      {SP_BTN_Y_MASK, BTN_NORTH},
    {SP_BTN_X_MASK, BTN_WEST},        {SP_BTN_B_MASK, BTN_SOUTH},     {SP_BTN_A_MASK, BTN_EAST},
    {SP_BTN_SELECT_MASK, BTN_SELECT}, {SP_BTN_START_MASK, BTN_START},
};

enum {
    SP_LEFT_BUTTON_MAP_COUNT = sizeof(SP_LEFT_BUTTON_MAP) / sizeof(SP_LEFT_BUTTON_MAP[0]),
    SP_RIGHT_BUTTON_MAP_COUNT = sizeof(SP_RIGHT_BUTTON_MAP) / sizeof(SP_RIGHT_BUTTON_MAP[0]),
};

static const unsigned short SP_KEYS[] = {
    BTN_EAST, BTN_SOUTH, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE,
};

static const struct gamepad_abs_desc SP_AXES[] = {
    SP_AXIS_DESC(ABS_X, -32767, 32767),  SP_AXIS_DESC(ABS_Y, -32767, 32767), SP_AXIS_DESC(ABS_Z, -32767, 32767),
    SP_AXIS_DESC(ABS_RZ, -32767, 32767), SP_AXIS_DESC(ABS_HAT0X, -1, 1),     SP_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

static const unsigned short SP_SWITCHES[] = {SW_TABLET_MODE};

static const struct gamepad_desc SMART_PRO_GAMEPAD_DESC = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = MUOS_INPUT_VENDOR,
            .product = MUOS_PRODUCT_TUI_SPOON,
            .version = MUOS_INPUT_VERSION,
        },
    .keys = SP_KEYS,
    .key_count = sizeof(SP_KEYS) / sizeof(SP_KEYS[0]),
    .axes = SP_AXES,
    .axis_count = sizeof(SP_AXES) / sizeof(SP_AXES[0]),
    .switches = SP_SWITCHES,
    .switch_count = sizeof(SP_SWITCHES) / sizeof(SP_SWITCHES[0]),
    .ff_effects_max = DEVICE_RUMBLE_EFFECT_SLOTS,
    .enable_ff_rumble = 1,
};
