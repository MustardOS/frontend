#pragma once

#include "ui_muxshare.h"
#include <lvgl/lvgl.h>

extern lv_obj_t *ui_lbl_code_webcode;
extern lv_obj_t *ui_pnl_expiry_webcode;
extern lv_obj_t *ui_bar_expiry_webcode;
extern lv_obj_t *ui_lbl_expiry_webcode;
extern lv_obj_t *ui_lbl_address_webcode;
extern lv_obj_t *ui_lbl_notice_webcode;

void init_muxwebcode(lv_obj_t *ui_pnl_content, const lv_font_t *code_font);
