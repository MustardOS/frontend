#pragma once

#include <stddef.h>

#include "../../common/uinput.h"
#include "smart_pro_s_structs.h"

enum {
    sp_s_btn_a_mask = 0x00000001,
    sp_s_btn_x_mask = 0x00000002,
    sp_s_btn_select_mask = 0x00000004,
    sp_s_btn_start_mask = 0x00000008,
    sp_s_btn_dpad_up_mask = 0x00000010,
    sp_s_btn_dpad_down_mask = 0x00000020,
    sp_s_btn_dpad_left_mask = 0x00000040,
    sp_s_btn_dpad_right_mask = 0x00000080,
    sp_s_btn_b_mask = 0x00000100,
    sp_s_btn_y_mask = 0x00000200,
    sp_s_btn_l1_mask = 0x00000400,
    sp_s_btn_r1_mask = 0x00000800,
    sp_s_btn_l2_mask = 0x00001000,
    sp_s_btn_r2_mask = 0x00002000,
    sp_s_btn_l3_mask = 0x00004000,
    sp_s_btn_r3_mask = 0x00008000,
    sp_s_btn_mode_mask = 0x00010000,
};

enum {
    sp_s_dpad_mask =
        sp_s_btn_dpad_up_mask | sp_s_btn_dpad_down_mask | sp_s_btn_dpad_left_mask | sp_s_btn_dpad_right_mask
};

enum {
    sp_s_gpio_left_enable = 332,
    sp_s_gpio_right_enable = 336,
    sp_s_gpio_input = 363,
};

#define SP_S_AXIS_DESC(_code, _min, _max)                                                                              \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct sp_s_button_map_entry sp_s_left_button_map[] = {
    {sp_s_btn_l1_mask, BTN_TL},
    {sp_s_btn_l2_mask, BTN_TL2},
    {sp_s_btn_l3_mask, BTN_THUMBL},
    {sp_s_btn_mode_mask, BTN_MODE},
};

static const struct sp_s_button_map_entry sp_s_right_button_map[] = {
    {sp_s_btn_a_mask, BTN_SOUTH},   {sp_s_btn_b_mask, BTN_EAST},      {sp_s_btn_y_mask, BTN_WEST},
    {sp_s_btn_x_mask, BTN_NORTH},   {sp_s_btn_r1_mask, BTN_TR},       {sp_s_btn_r2_mask, BTN_TR2},
    {sp_s_btn_r3_mask, BTN_THUMBR}, {sp_s_btn_start_mask, BTN_START}, {sp_s_btn_select_mask, BTN_SELECT},
};

enum {
    sp_s_left_button_map_count = sizeof(sp_s_left_button_map) / sizeof(sp_s_left_button_map[0]),
    sp_s_right_button_map_count = sizeof(sp_s_right_button_map) / sizeof(sp_s_right_button_map[0]),
};

static const unsigned short sp_s_keys[] = {BTN_SOUTH,  BTN_EAST, BTN_WEST,   BTN_NORTH,  BTN_TL,  BTN_TR, BTN_START,
                                           BTN_SELECT, BTN_MODE, BTN_THUMBR, BTN_THUMBL, BTN_TL2, BTN_TR2};

static const struct gamepad_abs_desc sp_s_axes[] = {
    SP_S_AXIS_DESC(ABS_X, -32767, 32767),  SP_S_AXIS_DESC(ABS_Y, -32767, 32767), SP_S_AXIS_DESC(ABS_RX, -32767, 32767),
    SP_S_AXIS_DESC(ABS_RY, -32767, 32767), SP_S_AXIS_DESC(ABS_HAT0X, -1, 1),     SP_S_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

static const unsigned short sp_s_switches[] = {SW_TABLET_MODE};

static const struct gamepad_desc smart_pro_s_gamepad_desc = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = muos_input_vendor,
            .product = muos_product_tui_smpro_s,
            .version = muos_input_version,
        },
    .keys = sp_s_keys,
    .key_count = sizeof(sp_s_keys) / sizeof(sp_s_keys[0]),
    .axes = sp_s_axes,
    .axis_count = sizeof(sp_s_axes) / sizeof(sp_s_axes[0]),
    .switches = sp_s_switches,
    .switch_count = sizeof(sp_s_switches) / sizeof(sp_s_switches[0]),

    .ff_effects_max = 0,
    .enable_ff_rumble = 0,
};
