#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxcustom(lv_obj_t *ui_pnlContent);

#define CUSTOM(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_custom; \
    extern lv_obj_t *ui_lbl##NAME##_custom; \
    extern lv_obj_t *ui_ico##NAME##_custom; \
    extern lv_obj_t *ui_dro##NAME##_custom;

CUSTOM_ELEMENTS
#undef CUSTOM
