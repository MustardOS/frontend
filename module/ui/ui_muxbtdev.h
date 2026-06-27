#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxbtdev(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

extern lv_obj_t *ui_pnl_entry_btdev;
extern lv_obj_t *ui_txt_entry_btdev;

#define BTDEV_INFO(NAME, UDATA)                                                                                        \
    extern lv_obj_t *ui_pnl_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_lbl_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_ico_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_val_##NAME##_btdev;

BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, UDATA)                                                                                         \
    extern lv_obj_t *ui_pnl_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_lbl_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_ico_##NAME##_btdev;                                                                            \
    extern lv_obj_t *ui_dro_##NAME##_btdev;

BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT
