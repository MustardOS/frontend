#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxcollect(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_lblCounter_collect;

extern lv_obj_t *ui_pnlEntry_collect;
extern lv_obj_t *ui_txtEntry_collect;
