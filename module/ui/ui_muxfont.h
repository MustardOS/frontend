#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxfont(lv_obj_t *ui_pnl_content);

#define FONT(NAME, UDATA)                                                                                              \
    extern lv_obj_t *ui_pnl_##NAME##_font;                                                                             \
    extern lv_obj_t *ui_lbl_##NAME##_font;                                                                             \
    extern lv_obj_t *ui_ico_##NAME##_font;                                                                             \
    extern lv_obj_t *ui_dro_##NAME##_font;

FONT_ELEMENTS
#undef FONT

extern const int font_size_values[];
extern const int font_size_count;
