#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxinfo(lv_obj_t *ui_pnlContent);

#define INFO(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_info; \
    extern lv_obj_t *ui_lbl##NAME##_info; \
    extern lv_obj_t *ui_ico##NAME##_info;

INFO_ELEMENTS
#undef INFO
