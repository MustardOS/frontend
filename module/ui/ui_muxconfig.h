#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxconfig(lv_obj_t *ui_pnlContent);

#define CONFIG(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_config; \
    extern lv_obj_t *ui_lbl##NAME##_config; \
    extern lv_obj_t *ui_ico##NAME##_config;

CONFIG_ELEMENTS
#undef CONFIG
