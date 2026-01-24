#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxconnect(lv_obj_t *ui_pnlContent);

#define CONNECT(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_connect; \
    extern lv_obj_t *ui_lbl##NAME##_connect; \
    extern lv_obj_t *ui_ico##NAME##_connect; \
    extern lv_obj_t *ui_dro##NAME##_connect;

CONNECT_ELEMENTS
#undef CONNECT
