#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxhdmi(lv_obj_t *ui_pnl_content);

#define HDMI(NAME, UDATA)                                                                                              \
    extern lv_obj_t *ui_pnl_##NAME##_hdmi;                                                                             \
    extern lv_obj_t *ui_lbl_##NAME##_hdmi;                                                                             \
    extern lv_obj_t *ui_ico_##NAME##_hdmi;                                                                             \
    extern lv_obj_t *ui_dro_##NAME##_hdmi;

HDMI_ELEMENTS
#undef HDMI
