#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxpower(lv_obj_t *ui_pnl_content);

#define POWER(NAME, UDATA)                                                                                             \
    extern lv_obj_t *ui_pnl_##NAME##_power;                                                                            \
    extern lv_obj_t *ui_lbl_##NAME##_power;                                                                            \
    extern lv_obj_t *ui_ico_##NAME##_power;                                                                            \
    extern lv_obj_t *ui_dro_##NAME##_power;

POWER_ELEMENTS
#undef POWER
