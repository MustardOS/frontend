#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxhdmi(lv_obj_t *ui_pnlContent);

#define HDMI(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_hdmi; \
    extern lv_obj_t *ui_lbl##NAME##_hdmi; \
    extern lv_obj_t *ui_ico##NAME##_hdmi; \
    extern lv_obj_t *ui_dro##NAME##_hdmi;

HDMI_ELEMENTS
#undef HDMI
