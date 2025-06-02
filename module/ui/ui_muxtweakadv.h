#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxtweakadv(lv_obj_t *ui_pnlContent);

#define TWEAKADV(NAME)                        \
    extern lv_obj_t *ui_pnl##NAME##_tweakadv; \
    extern lv_obj_t *ui_lbl##NAME##_tweakadv; \
    extern lv_obj_t *ui_ico##NAME##_tweakadv; \
    extern lv_obj_t *ui_dro##NAME##_tweakadv;

TWEAKADV_ELEMENTS
#undef TWEAKADV

extern const char *volume_values[];

extern const char *brightness_values[];

extern const char *state_values[];

extern const char *cardmode_values[];

extern const int accelerate_values[];

extern const int swap_values[];

extern const int zram_values[];
