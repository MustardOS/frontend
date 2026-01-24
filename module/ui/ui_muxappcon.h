#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxappcon(lv_obj_t *ui_pnlContent);

#define APPCON(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_appcon;      \
    extern lv_obj_t *ui_lbl##NAME##_appcon;      \
    extern lv_obj_t *ui_ico##NAME##_appcon;      \
    extern lv_obj_t *ui_lbl##NAME##Value_appcon;

APPCON_ELEMENTS
#undef APPCON
