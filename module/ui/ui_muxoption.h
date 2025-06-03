#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxoption(lv_obj_t *ui_pnlContent);

#define OPTION(NAME)                        \
    extern lv_obj_t *ui_pnl##NAME##_option; \
    extern lv_obj_t *ui_lbl##NAME##_option; \
    extern lv_obj_t *ui_ico##NAME##_option; \
    extern lv_obj_t *ui_lbl##NAME##Value_option;

OPTION_ELEMENTS
#undef OPTION
