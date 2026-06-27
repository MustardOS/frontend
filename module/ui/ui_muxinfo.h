#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxinfo(lv_obj_t *ui_pnl_content);

#define INFO(NAME, UDATA)                                                                                              \
    extern lv_obj_t *ui_pnl_##NAME##_info;                                                                             \
    extern lv_obj_t *ui_lbl_##NAME##_info;                                                                             \
    extern lv_obj_t *ui_ico_##NAME##_info;

INFO_ELEMENTS
#undef INFO
