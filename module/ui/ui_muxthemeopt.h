#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxthemeopt(lv_obj_t *ui_pnlContent);

#define THEMEOPT(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_themeopt; \
    extern lv_obj_t *ui_lbl##NAME##_themeopt; \
    extern lv_obj_t *ui_ico##NAME##_themeopt; \
    extern lv_obj_t *ui_dro##NAME##_themeopt;

THEMEOPT_ELEMENTS
#undef THEMEOPT
