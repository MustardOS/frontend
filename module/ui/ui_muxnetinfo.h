#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define NETINFO(NAME, UDATA)                      \
    extern lv_obj_t *ui_pnl##NAME##_netinfo;      \
    extern lv_obj_t *ui_lbl##NAME##_netinfo;      \
    extern lv_obj_t *ui_ico##NAME##_netinfo;      \
    extern lv_obj_t *ui_lbl##NAME##Value_netinfo;

NETINFO_ELEMENTS
#undef NETINFO

extern lv_obj_t *ui_pnlEntry_netinfo;
extern lv_obj_t *ui_txtEntry_netinfo;
