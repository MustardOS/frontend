#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxspace(lv_obj_t *ui_pnl_content);

#define SPACE(NAME, UDATA)                                                                                             \
    extern lv_obj_t *ui_pnl_##NAME##_space;                                                                            \
    extern lv_obj_t *ui_lbl_##NAME##_space;                                                                            \
    extern lv_obj_t *ui_ico_##NAME##_space;                                                                            \
    extern lv_obj_t *ui_val_##NAME##_space;                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_bar_space;                                                                        \
    extern lv_obj_t *ui_bar_##NAME##_space;

SPACE_ELEMENTS
#undef SPACE
