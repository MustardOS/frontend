#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxvisual(lv_obj_t *ui_pnlContent);

#define VISUAL(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_visual; \
    extern lv_obj_t *ui_lbl##NAME##_visual; \
    extern lv_obj_t *ui_ico##NAME##_visual; \
    extern lv_obj_t *ui_dro##NAME##_visual;

VISUAL_ELEMENTS
#undef VISUAL
