#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxvisual(lv_obj_t *ui_pnl_content);

#define VISUAL(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_visual;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_visual;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_visual;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_visual;

VISUAL_ELEMENTS
#undef VISUAL
