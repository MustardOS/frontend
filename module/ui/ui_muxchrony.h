#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxchrony(lv_obj_t *ui_pnl_content);

#define CHRONY(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_chrony;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_chrony;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_chrony;                                                                           \
    extern lv_obj_t *ui_val_##NAME##_chrony;

CHRONY_ELEMENTS
#undef CHRONY
