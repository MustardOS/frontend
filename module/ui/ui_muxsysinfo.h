#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxsysinfo(lv_obj_t *ui_pnl_content);

#define SYSINFO(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_sysinfo;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_sysinfo;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_sysinfo;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_sysinfo;

SYSINFO_ELEMENTS
#undef SYSINFO
