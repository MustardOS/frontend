#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"

void init_muxcontent(lv_obj_t *ui_pnlContent);

#define CONTENT(NAME, ENUM, UDATA)           \
    extern lv_obj_t *ui_pnl##NAME##_content; \
    extern lv_obj_t *ui_lbl##NAME##_content; \
    extern lv_obj_t *ui_ico##NAME##_content; \
    extern lv_obj_t *ui_dro##NAME##_content;

CONTENT_ELEMENTS
#undef CONTENT
