#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxdetail(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

extern lv_obj_t *ui_pnl_entry_detail;
extern lv_obj_t *ui_txt_entry_detail;

#define DETAIL(NAME, UDATA)                                                                                            \
    extern lv_obj_t *ui_pnl_##NAME##_detail;                                                                           \
    extern lv_obj_t *ui_lbl_##NAME##_detail;                                                                           \
    extern lv_obj_t *ui_ico_##NAME##_detail;                                                                           \
    extern lv_obj_t *ui_val_##NAME##_detail;

DETAIL_ELEMENTS
#undef DETAIL
