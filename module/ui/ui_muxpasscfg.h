#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"
#include "ui_muxshare.h"

void init_muxpasscfg(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

extern lv_obj_t *ui_pnl_entry_passcfg;
extern lv_obj_t *ui_txt_entry_passcfg;

#define PASSCFG(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_passcfg;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_passcfg;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_passcfg;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_passcfg;

PASSCFG_ELEMENTS
#undef PASSCFG
