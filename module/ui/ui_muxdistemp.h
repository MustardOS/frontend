#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdistemp(lv_obj_t *ui_pnlContent);

#define DISTEMP(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_distemp; \
    extern lv_obj_t *ui_lbl##NAME##_distemp; \
    extern lv_obj_t *ui_ico##NAME##_distemp; \
    extern lv_obj_t *ui_dro##NAME##_distemp;

DISTEMP_ELEMENTS
#undef DISTEMP
