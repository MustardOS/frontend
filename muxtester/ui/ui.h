#pragma once

#include "../../lvgl/lvgl.h"

void ui_scrTester_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_lblButton;
extern lv_obj_t *ui_lblFirst;

LV_FONT_DECLARE(ui_font_Gamepad);

void ui_init(lv_obj_t *ui_pnlContent);
