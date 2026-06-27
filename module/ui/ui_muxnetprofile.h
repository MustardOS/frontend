#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define NETWORK(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_network;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_network;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_network;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_network;

NETWORK_ELEMENTS
#undef NETWORK

extern lv_obj_t *ui_pnl_entry_network;
extern lv_obj_t *ui_txt_entry_network;
