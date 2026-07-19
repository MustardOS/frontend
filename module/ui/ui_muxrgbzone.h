#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxrgbzone(lv_obj_t *ui_pnl_content);

#define RGBZONE(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_rgbzone;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_rgbzone;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_rgbzone;                                                                          \
    extern lv_obj_t *ui_dro_##NAME##_rgbzone;

RGBZONE_ELEMENTS
#undef RGBZONE
