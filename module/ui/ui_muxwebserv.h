#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxwebserv(lv_obj_t *ui_pnlContent);

#define WEBSERV(NAME)                        \
    extern lv_obj_t *ui_pnl##NAME##_webserv; \
    extern lv_obj_t *ui_lbl##NAME##_webserv; \
    extern lv_obj_t *ui_ico##NAME##_webserv; \
    extern lv_obj_t *ui_dro##NAME##_webserv;

WEBSERV_ELEMENTS
#undef WEBSERV
