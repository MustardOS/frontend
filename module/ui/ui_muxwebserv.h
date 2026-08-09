#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxwebserv(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme);

#define WEBSERV(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_val_##NAME##_webserv;

WEBSERV_ELEMENTS
#undef WEBSERV

extern lv_obj_t *ui_pnl_entry_webserv;
extern lv_obj_t *ui_txt_entry_webserv;
