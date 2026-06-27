#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define NETINFO(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_netinfo;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_netinfo;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_netinfo;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_netinfo;

NETINFO_ELEMENTS
#undef NETINFO

extern lv_obj_t *ui_pnl_entry_netinfo;
extern lv_obj_t *ui_txt_entry_netinfo;
