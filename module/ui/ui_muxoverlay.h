#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxoverlay(lv_obj_t *ui_pnl_content);

#define OVERLAY(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_overlay;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_overlay;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_overlay;                                                                          \
    extern lv_obj_t *ui_dro_##NAME##_overlay;

OVERLAY_ELEMENTS
#undef OVERLAY
