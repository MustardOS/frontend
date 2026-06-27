#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxdanger(lv_obj_t *ui_pnl_content);

#define DANGER(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_danger;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_danger;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_danger;                                                                           \
    extern lv_obj_t *ui_dro_##NAME##_danger;

DANGER_ELEMENTS
#undef DANGER

extern const int four_values[];

extern const int merge_values[];

extern const int request_values[];

extern const int read_ahead_values[];

extern const int time_slice_values[];

extern const char *cardmode_values[];

extern const char *state_values[];
