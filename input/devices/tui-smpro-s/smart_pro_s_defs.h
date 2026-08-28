#pragma once

#include <stddef.h>

#include "../../common/uinput.h"
#include "smart_pro_s_structs.h"

enum {
    SP_S_BTN_A_MASK = 0x00000001,
    SP_S_BTN_X_MASK = 0x00000002,
    SP_S_BTN_SELECT_MASK = 0x00000004,
    SP_S_BTN_START_MASK = 0x00000008,
    SP_S_BTN_DPAD_UP_MASK = 0x00000010,
    SP_S_BTN_DPAD_DOWN_MASK = 0x00000020,
    SP_S_BTN_DPAD_LEFT_MASK = 0x00000040,
    SP_S_BTN_DPAD_RIGHT_MASK = 0x00000080,
    SP_S_BTN_B_MASK = 0x00000100,
    SP_S_BTN_Y_MASK = 0x00000200,
    SP_S_BTN_L1_MASK = 0x00000400,
    SP_S_BTN_R1_MASK = 0x00000800,
    SP_S_BTN_L2_MASK = 0x00001000,
    SP_S_BTN_R2_MASK = 0x00002000,
    SP_S_BTN_L3_MASK = 0x00004000,
    SP_S_BTN_R3_MASK = 0x00008000,
    SP_S_BTN_MODE_MASK = 0x00010000,
};

enum {
    SP_S_DPAD_MASK =
        SP_S_BTN_DPAD_UP_MASK | SP_S_BTN_DPAD_DOWN_MASK | SP_S_BTN_DPAD_LEFT_MASK | SP_S_BTN_DPAD_RIGHT_MASK
};

enum {
    SP_S_GPIO_LEFT_ENABLE = 332,
    SP_S_GPIO_RIGHT_ENABLE = 336,
    SP_S_GPIO_INPUT = 363,
};

#define SP_S_AXIS_DESC(_code, _min, _max)                                                                              \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct sp_s_button_map_entry SP_S_LEFT_BUTTON_MAP[] = {
    {SP_S_BTN_L1_MASK, BTN_TL},
    {SP_S_BTN_L2_MASK, BTN_TL2},
    {SP_S_BTN_L3_MASK, BTN_THUMBL},
    {SP_S_BTN_MODE_MASK, BTN_MODE},
};

static const struct sp_s_button_map_entry SP_S_RIGHT_BUTTON_MAP[] = {
    {SP_S_BTN_A_MASK, BTN_SOUTH},   {SP_S_BTN_B_MASK, BTN_EAST},      {SP_S_BTN_Y_MASK, BTN_WEST},
    {SP_S_BTN_X_MASK, BTN_NORTH},   {SP_S_BTN_R1_MASK, BTN_TR},       {SP_S_BTN_R2_MASK, BTN_TR2},
    {SP_S_BTN_R3_MASK, BTN_THUMBR}, {SP_S_BTN_START_MASK, BTN_START}, {SP_S_BTN_SELECT_MASK, BTN_SELECT},
};

enum {
    SP_S_LEFT_BUTTON_MAP_COUNT = sizeof(SP_S_LEFT_BUTTON_MAP) / sizeof(SP_S_LEFT_BUTTON_MAP[0]),
    SP_S_RIGHT_BUTTON_MAP_COUNT = sizeof(SP_S_RIGHT_BUTTON_MAP) / sizeof(SP_S_RIGHT_BUTTON_MAP[0]),
};

static const unsigned short SP_S_KEYS[] = {BTN_SOUTH,  BTN_EAST, BTN_WEST,   BTN_NORTH,  BTN_TL,  BTN_TR, BTN_START,
                                           BTN_SELECT, BTN_MODE, BTN_THUMBR, BTN_THUMBL, BTN_TL2, BTN_TR2};

static const struct gamepad_abs_desc SP_S_AXES[] = {
    SP_S_AXIS_DESC(ABS_X, -32767, 32767),  SP_S_AXIS_DESC(ABS_Y, -32767, 32767), SP_S_AXIS_DESC(ABS_RX, -32767, 32767),
    SP_S_AXIS_DESC(ABS_RY, -32767, 32767), SP_S_AXIS_DESC(ABS_HAT0X, -1, 1),     SP_S_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

static const unsigned short SP_S_SWITCHES[] = {SW_TABLET_MODE};

static const struct gamepad_desc SMART_PRO_S_GAMEPAD_DESC = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = MUOS_INPUT_VENDOR,
            .product = MUOS_PRODUCT_TUI_SMPRO_S,
            .version = MUOS_INPUT_VERSION,
        },
    .keys = SP_S_KEYS,
    .key_count = sizeof(SP_S_KEYS) / sizeof(SP_S_KEYS[0]),
    .axes = SP_S_AXES,
    .axis_count = sizeof(SP_S_AXES) / sizeof(SP_S_AXES[0]),
    .switches = SP_S_SWITCHES,
    .switch_count = sizeof(SP_S_SWITCHES) / sizeof(SP_S_SWITCHES[0]),

    .ff_effects_max = 0,
    .enable_ff_rumble = 0,
};
