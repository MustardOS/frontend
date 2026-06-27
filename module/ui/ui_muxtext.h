#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxtext(lv_obj_t *ui_screen, const struct theme_config *theme);

extern lv_obj_t *ui_pnl_document_text;

extern lv_obj_t *ui_txt_document_text;
