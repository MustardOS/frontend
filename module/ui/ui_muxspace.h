#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxspace(lv_obj_t *ui_pnlContent);

#define SPACE(NAME, UDATA)                      \
    extern lv_obj_t *ui_pnl##NAME##_space;      \
    extern lv_obj_t *ui_lbl##NAME##_space;      \
    extern lv_obj_t *ui_ico##NAME##_space;      \
    extern lv_obj_t *ui_lbl##NAME##Value_space; \
    extern lv_obj_t *ui_pnl##NAME##Bar_space;   \
    extern lv_obj_t *ui_bar##NAME##_space;

SPACE_ELEMENTS
#undef SPACE
