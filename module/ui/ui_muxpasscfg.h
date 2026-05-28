#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"
#include "ui_muxshare.h"

void init_muxpasscfg(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_pnlEntry_passcfg;
extern lv_obj_t *ui_txtEntry_passcfg;

#define PASSCFG(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_passcfg;      \
    extern lv_obj_t *ui_lbl##NAME##_passcfg;      \
    extern lv_obj_t *ui_ico##NAME##_passcfg;      \
    extern lv_obj_t *ui_lbl##NAME##Value_passcfg;

PASSCFG_ELEMENTS
#undef PASSCFG
