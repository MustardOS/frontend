#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdanger(lv_obj_t *ui_pnlContent);

#define DANGER(NAME, UDATA)                 \
    extern lv_obj_t *ui_pnl##NAME##_danger; \
    extern lv_obj_t *ui_lbl##NAME##_danger; \
    extern lv_obj_t *ui_ico##NAME##_danger; \
    extern lv_obj_t *ui_dro##NAME##_danger;

DANGER_ELEMENTS
#undef DANGER

extern const int four_values[];

extern const int merge_values[];

extern const int request_values[];

extern const int read_ahead_values[];
