#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxbatinfo(const lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define BATINFO(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_batinfo;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_batinfo;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_batinfo;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_batinfo;

BATINFO_ELEMENTS
#undef BATINFO
