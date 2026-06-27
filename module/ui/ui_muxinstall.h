#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxinstall(lv_obj_t *ui_pnl_content);

#define INSTALL(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_install;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_install;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_install;

INSTALL_ELEMENTS
#undef INSTALL
