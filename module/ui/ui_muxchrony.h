#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxchrony(lv_obj_t *ui_pnlContent);

#define CHRONY(NAME, UDATA)                     \
    extern lv_obj_t *ui_pnl##NAME##_chrony;      \
    extern lv_obj_t *ui_lbl##NAME##_chrony;      \
    extern lv_obj_t *ui_ico##NAME##_chrony;      \
    extern lv_obj_t *ui_lbl##NAME##Value_chrony;

CHRONY_ELEMENTS
#undef CHRONY
