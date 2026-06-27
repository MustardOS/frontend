#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxthemeopt(lv_obj_t *ui_pnl_content);

#define THEMEOPT(NAME, UDATA)                                                                                          \
    extern lv_obj_t *ui_pnl_##NAME##_themeopt;                                                                         \
    extern lv_obj_t *ui_lbl_##NAME##_themeopt;                                                                         \
    extern lv_obj_t *ui_ico_##NAME##_themeopt;                                                                         \
    extern lv_obj_t *ui_dro_##NAME##_themeopt;

THEMEOPT_ELEMENTS
#undef THEMEOPT
