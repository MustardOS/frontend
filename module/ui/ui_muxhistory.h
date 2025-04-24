#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

extern lv_obj_t *ui_lblCounter_history;

void init_muxhistory(lv_obj_t *ui_screen, struct theme_config *theme);
