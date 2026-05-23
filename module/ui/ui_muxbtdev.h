#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxbtdev(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_pnlEntry_btdev;
extern lv_obj_t *ui_txtEntry_btdev;

#define BTDEV_INFO(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_btdev;      \
    extern lv_obj_t *ui_lbl##NAME##_btdev;      \
    extern lv_obj_t *ui_ico##NAME##_btdev;      \
    extern lv_obj_t *ui_lbl##NAME##Value_btdev;

BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, ENUM, UDATA)       \
    extern lv_obj_t *ui_pnl##NAME##_btdev; \
    extern lv_obj_t *ui_lbl##NAME##_btdev; \
    extern lv_obj_t *ui_ico##NAME##_btdev; \
    extern lv_obj_t *ui_dro##NAME##_btdev;

BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT
