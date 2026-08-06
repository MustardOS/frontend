#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxaccess(lv_obj_t *ui_pnl_content);

#define ACCESS(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_access;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_access;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_access;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_access;

ACCESS_ELEMENTS
#undef ACCESS
