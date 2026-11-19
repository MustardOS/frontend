#pragma once

#include "ui_muxshare.h"
#include <lvgl/lvgl.h>

void init_muxmenuadjust(lv_obj_t *ui_pnl_content);

#define MENUADJUST(NAME, UDATA)                                                                                        \
    extern lv_obj_t *ui_pnl_##NAME##_menuadjust;                                                                       \
    extern lv_obj_t *ui_lbl_##NAME##_menuadjust;                                                                       \
    extern lv_obj_t *ui_ico_##NAME##_menuadjust;                                                                       \
    extern lv_obj_t *ui_dro_##NAME##_menuadjust;

MENUADJUST_ELEMENTS
#undef MENUADJUST
