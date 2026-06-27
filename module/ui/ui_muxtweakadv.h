#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxtweakadv(lv_obj_t *ui_pnl_content);

#define TWEAKADV(NAME, UDATA)                                                                                          \
    extern lv_obj_t *ui_pnl_##NAME##_tweakadv;                                                                         \
    extern lv_obj_t *ui_lbl_##NAME##_tweakadv;                                                                         \
    extern lv_obj_t *ui_ico_##NAME##_tweakadv;                                                                         \
    extern lv_obj_t *ui_dro_##NAME##_tweakadv;

TWEAKADV_ELEMENTS
#undef TWEAKADV

extern const int accelerate_values[];

extern const int repeat_delay_values[];

extern const int swap_values[];

extern const int battery_offset_values[];
