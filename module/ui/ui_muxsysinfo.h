#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxsysinfo(lv_obj_t *ui_pnlContent);

#define SYSINFO(NAME, UDATA)                      \
    extern lv_obj_t *ui_pnl##NAME##_sysinfo;      \
    extern lv_obj_t *ui_lbl##NAME##_sysinfo;      \
    extern lv_obj_t *ui_ico##NAME##_sysinfo;      \
    extern lv_obj_t *ui_lbl##NAME##Value_sysinfo;

SYSINFO_ELEMENTS
#undef SYSINFO
