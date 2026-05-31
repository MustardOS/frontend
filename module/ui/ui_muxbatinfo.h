#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxbatinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define BATINFO(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_batinfo;      \
    extern lv_obj_t *ui_lbl##NAME##_batinfo;      \
    extern lv_obj_t *ui_ico##NAME##_batinfo;      \
    extern lv_obj_t *ui_lbl##NAME##Value_batinfo;

BATINFO_ELEMENTS
#undef BATINFO
