#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxcoladjust(lv_obj_t *ui_pnlContent);

#define COLADJUST(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_coladjust; \
    extern lv_obj_t *ui_lbl##NAME##_coladjust; \
    extern lv_obj_t *ui_ico##NAME##_coladjust; \
    extern lv_obj_t *ui_dro##NAME##_coladjust;

COLADJUST_ELEMENTS
#undef COLADJUST
