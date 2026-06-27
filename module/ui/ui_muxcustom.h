#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxcustom(lv_obj_t *ui_pnl_content);

#define CUSTOM(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_custom;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_custom;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_custom;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_custom;

CUSTOM_ELEMENTS
#undef CUSTOM
