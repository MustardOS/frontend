#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxoverlay(lv_obj_t *ui_pnlContent);

#define OVERLAY(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_overlay; \
    extern lv_obj_t *ui_lbl##NAME##_overlay; \
    extern lv_obj_t *ui_ico##NAME##_overlay; \
    extern lv_obj_t *ui_dro##NAME##_overlay;

OVERLAY_ELEMENTS
#undef OVERLAY
