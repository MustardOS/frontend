#pragma once

#include <stddef.h>

#include "../../common/uinput.h"
#include "smart_pro_structs.h"
#include "../device_rumble.h"

enum {
    sp_btn_l1_mask = 0x01,
    sp_btn_l2_mask = 0x02,
    sp_btn_dpad_up_mask = 0x04,
    sp_btn_dpad_left_mask = 0x08,
    sp_btn_dpad_right_mask = 0x10,
    sp_btn_dpad_down_mask = 0x20,
    sp_btn_mode_mask = 0x80,
};

enum {
    sp_btn_r1_mask = 0x01,
    sp_btn_r2_mask = 0x02,
    sp_btn_y_mask = 0x04,
    sp_btn_x_mask = 0x08,
    sp_btn_b_mask = 0x10,
    sp_btn_a_mask = 0x20,
    sp_btn_select_mask = 0x40,
    sp_btn_start_mask = 0x80,
};

enum { sp_dpad_mask = sp_btn_dpad_up_mask | sp_btn_dpad_down_mask | sp_btn_dpad_left_mask | sp_btn_dpad_right_mask };

enum {
    sp_gpio_out1 = 110,
    sp_gpio_out2 = 114,
    sp_gpio_input = 243,
};

#define SP_AXIS_DESC(_code, _min, _max)                                                                                \
    {.code = (_code), .min = (_min), .max = (_max), .flat = 0, .fuzz = 0, .resolution = 0}

static const struct sp_button_map_entry sp_left_button_map[] = {
    {sp_btn_l1_mask, BTN_TL},
    {sp_btn_l2_mask, BTN_TL2},
    {sp_btn_mode_mask, BTN_MODE},
};

static const struct sp_button_map_entry sp_right_button_map[] = {
    {sp_btn_r1_mask, BTN_TR},         {sp_btn_r2_mask, BTN_TR2},      {sp_btn_y_mask, BTN_NORTH},
    {sp_btn_x_mask, BTN_WEST},        {sp_btn_b_mask, BTN_SOUTH},     {sp_btn_a_mask, BTN_EAST},
    {sp_btn_select_mask, BTN_SELECT}, {sp_btn_start_mask, BTN_START},
};

enum {
    sp_left_button_map_count = sizeof(sp_left_button_map) / sizeof(sp_left_button_map[0]),
    sp_right_button_map_count = sizeof(sp_right_button_map) / sizeof(sp_right_button_map[0]),
};

static const unsigned short sp_keys[] = {
    BTN_EAST, BTN_SOUTH, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE,
};

static const struct gamepad_abs_desc sp_axes[] = {
    SP_AXIS_DESC(ABS_X, -32767, 32767),  SP_AXIS_DESC(ABS_Y, -32767, 32767), SP_AXIS_DESC(ABS_Z, -32767, 32767),
    SP_AXIS_DESC(ABS_RZ, -32767, 32767), SP_AXIS_DESC(ABS_HAT0X, -1, 1),     SP_AXIS_DESC(ABS_HAT0Y, -1, 1),
};

static const unsigned short sp_switches[] = {SW_TABLET_MODE};

static const struct gamepad_desc smart_pro_gamepad_desc = {
    .name = MUOS_GAMEPAD_NAME,
    .id =
        {
            .bustype = BUS_VIRTUAL,
            .vendor = muos_input_vendor,
            .product = muos_product_tui_spoon,
            .version = muos_input_version,
        },
    .keys = sp_keys,
    .key_count = sizeof(sp_keys) / sizeof(sp_keys[0]),
    .axes = sp_axes,
    .axis_count = sizeof(sp_axes) / sizeof(sp_axes[0]),
    .switches = sp_switches,
    .switch_count = sizeof(sp_switches) / sizeof(sp_switches[0]),
    .ff_effects_max = device_rumble_effect_slots,
    .enable_ff_rumble = 1,
};
