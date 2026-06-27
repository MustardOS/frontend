#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxlaunch(lv_obj_t *ui_pnl_content);

#define LAUNCH(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_launch;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_launch;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_launch;

LAUNCH_ELEMENTS
#undef LAUNCH
