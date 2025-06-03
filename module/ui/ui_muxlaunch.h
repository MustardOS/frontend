#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxlaunch(lv_obj_t *ui_pnlContent);

#define LAUNCH(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_launch; \
    extern lv_obj_t *ui_lbl##NAME##_launch; \
    extern lv_obj_t *ui_ico##NAME##_launch;

LAUNCH_ELEMENTS
#undef LAUNCH
