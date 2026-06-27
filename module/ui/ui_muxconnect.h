#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxconnect(lv_obj_t *ui_pnl_content);

#define CONNECT(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_connect;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_connect;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_connect;                                                                          \
    extern lv_obj_t *ui_dro_##NAME##_connect;

CONNECT_ELEMENTS
#undef CONNECT
