#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetproxy(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define PROXY(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_proxy;      \
    extern lv_obj_t *ui_lbl##NAME##_proxy;      \
    extern lv_obj_t *ui_ico##NAME##_proxy;      \
    extern lv_obj_t *ui_lbl##NAME##Value_proxy;

PROXY_ELEMENTS
#undef PROXY

extern lv_obj_t *ui_pnlEntry_proxy;
extern lv_obj_t *ui_txtEntry_proxy;
