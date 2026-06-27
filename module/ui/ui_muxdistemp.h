#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdistemp(lv_obj_t *ui_pnl_content);

#define DISTEMP(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_distemp;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_distemp;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_distemp;                                                                          \
    extern lv_obj_t *ui_dro_##NAME##_distemp;

DISTEMP_ELEMENTS
#undef DISTEMP
