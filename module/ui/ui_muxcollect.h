#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxcollect(lv_obj_t *ui_screen, const struct theme_config *theme);

extern lv_obj_t *ui_lbl_counter_collect;

extern lv_obj_t *ui_pnl_entry_collect;
extern lv_obj_t *ui_txt_entry_collect;
