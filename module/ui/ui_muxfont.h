#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxfont(lv_obj_t *ui_pnlContent);

#define FONT(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_font; \
    extern lv_obj_t *ui_lbl##NAME##_font; \
    extern lv_obj_t *ui_ico##NAME##_font; \
    extern lv_obj_t *ui_dro##NAME##_font;

FONT_ELEMENTS
#undef FONT

extern const int font_size_values[];
extern const int font_size_count;
