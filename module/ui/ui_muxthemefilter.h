#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxthemefilter(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define THEMEFILTER(NAME, UDATA)                                                                                       \
    extern lv_obj_t *ui_pnl_##NAME##_themefilter;                                                                      \
    extern lv_obj_t *ui_lbl_##NAME##_themefilter;                                                                      \
    extern lv_obj_t *ui_ico_##NAME##_themefilter;                                                                      \
    extern lv_obj_t *ui_dro_##NAME##_themefilter;

THEMEFILTER_ELEMENTS
#undef THEMEFILTER

extern lv_obj_t *ui_pnl_lookup_themefilter;
extern lv_obj_t *ui_lbl_lookup_themefilter;
extern lv_obj_t *ui_ico_lookup_themefilter;
extern lv_obj_t *ui_val_lookup_themefilter;

extern lv_obj_t *ui_pnl_entry_themefilter;
extern lv_obj_t *ui_txt_entry_themefilter;
