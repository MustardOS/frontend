#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxsearch(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define SEARCH(NAME)                             \
    extern lv_obj_t *ui_pnl##NAME##_search;      \
    extern lv_obj_t *ui_lbl##NAME##_search;      \
    extern lv_obj_t *ui_ico##NAME##_search;      \
    extern lv_obj_t *ui_lbl##NAME##Value_search;

SEARCH_ELEMENTS
#undef SEARCH

extern lv_obj_t *ui_pnlEntry_search;
extern lv_obj_t *ui_txtEntry_search;
