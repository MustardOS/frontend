#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxrgb(lv_obj_t *ui_pnlContent);

#define RGB(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_rgb; \
    extern lv_obj_t *ui_lbl##NAME##_rgb; \
    extern lv_obj_t *ui_ico##NAME##_rgb; \
    extern lv_obj_t *ui_dro##NAME##_rgb;

RGB_ELEMENTS
#undef RGB
