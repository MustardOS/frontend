#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxpower(lv_obj_t *ui_pnlContent);

#define POWER(NAME)                        \
    extern lv_obj_t *ui_pnl##NAME##_power; \
    extern lv_obj_t *ui_lbl##NAME##_power; \
    extern lv_obj_t *ui_ico##NAME##_power; \
    extern lv_obj_t *ui_dro##NAME##_power;

POWER_ELEMENTS
#undef POWER
