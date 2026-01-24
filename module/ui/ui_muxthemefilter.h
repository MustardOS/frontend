#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxthemefilter(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

#define THEMEFILTER(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_themefilter; \
    extern lv_obj_t *ui_lbl##NAME##_themefilter; \
    extern lv_obj_t *ui_ico##NAME##_themefilter; \
    extern lv_obj_t *ui_dro##NAME##_themefilter;

THEMEFILTER_ELEMENTS
#undef THEMEFILTER

extern lv_obj_t *ui_pnlLookup_themefilter;
extern lv_obj_t *ui_lblLookup_themefilter;
extern lv_obj_t *ui_icoLookup_themefilter;
extern lv_obj_t *ui_lblLookupValue_themefilter;
extern lv_obj_t *ui_pnlEntry_themefilter;
extern lv_obj_t *ui_txtEntry_themefilter;
