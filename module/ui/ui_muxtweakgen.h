#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxtweakgen(lv_obj_t *ui_pnlContent);

#define TWEAKGEN(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_tweakgen; \
    extern lv_obj_t *ui_lbl##NAME##_tweakgen; \
    extern lv_obj_t *ui_ico##NAME##_tweakgen; \
    extern lv_obj_t *ui_dro##NAME##_tweakgen;

TWEAKGEN_ELEMENTS
#undef TWEAKGEN
