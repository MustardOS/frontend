#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxsearch(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define SEARCH(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_search;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_search;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_search;                                                                           \
    extern lv_obj_t *ui_val_##NAME##_search;

SEARCH_ELEMENTS
#undef SEARCH

extern lv_obj_t *ui_pnl_entry_search;
extern lv_obj_t *ui_txt_entry_search;
