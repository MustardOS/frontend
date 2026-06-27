#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxwebserv(lv_obj_t *ui_pnl_content);

#define WEBSERV(NAME, UDATA)                                                                                           \
    extern lv_obj_t *ui_pnl_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_lbl_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_ico_##NAME##_webserv;                                                                          \
    extern lv_obj_t *ui_dro_##NAME##_webserv;

WEBSERV_ELEMENTS
#undef WEBSERV
