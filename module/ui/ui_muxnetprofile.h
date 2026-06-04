#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define NETWORK(NAME, ENUM, UDATA)                \
    extern lv_obj_t *ui_pnl##NAME##_network;      \
    extern lv_obj_t *ui_lbl##NAME##_network;      \
    extern lv_obj_t *ui_ico##NAME##_network;      \
    extern lv_obj_t *ui_lbl##NAME##Value_network;

NETWORK_ELEMENTS
#undef NETWORK

extern lv_obj_t *ui_pnlEntry_network;
extern lv_obj_t *ui_txtEntry_network;
