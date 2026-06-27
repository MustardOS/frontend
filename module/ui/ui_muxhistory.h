#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

extern lv_obj_t *ui_lbl_counter_history;

void init_muxhistory(lv_obj_t *ui_screen, const struct theme_config *theme);
