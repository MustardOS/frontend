#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxsort(lv_obj_t *ui_pnlContent);

#define SORT(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_sort; \
    extern lv_obj_t *ui_lbl##NAME##_sort; \
    extern lv_obj_t *ui_ico##NAME##_sort; \
    extern lv_obj_t *ui_dro##NAME##_sort;

SORT_ELEMENTS
#undef SORT
