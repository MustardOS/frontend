#pragma once

#include "ui_muxshare.h"
#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxtext(lv_obj_t *ui_screen, struct theme_config *theme);

extern lv_obj_t *ui_pnlDocument_text;

extern lv_obj_t *ui_txtDocument_text;
