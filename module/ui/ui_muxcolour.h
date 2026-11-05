#pragma once

#include "ui_muxshare.h"
#include <lvgl/lvgl.h>

void init_muxcolour(lv_obj_t *ui_pnl_content);

#define COLOUR(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_colour;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_colour;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_colour;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_colour;

COLOUR_ELEMENTS
#undef COLOUR
