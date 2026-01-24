#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxinstall(lv_obj_t *ui_pnlContent);

#define INSTALL(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_install; \
    extern lv_obj_t *ui_lbl##NAME##_install; \
    extern lv_obj_t *ui_ico##NAME##_install;

INSTALL_ELEMENTS
#undef INSTALL
