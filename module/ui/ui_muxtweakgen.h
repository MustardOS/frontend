#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxtweakgen(lv_obj_t *ui_pnl_content);

#define TWEAKGEN(NAME, UDATA)                                                                                          \
    extern lv_obj_t *ui_pnl_##NAME##_tweakgen;                                                                         \
    extern lv_obj_t *ui_lbl_##NAME##_tweakgen;                                                                         \
    extern lv_obj_t *ui_ico_##NAME##_tweakgen;                                                                         \
    extern lv_obj_t *ui_dro_##NAME##_tweakgen;

TWEAKGEN_ELEMENTS
#undef TWEAKGEN
