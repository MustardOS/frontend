#pragma once

#include "../../lvgl/lvgl.h"

void ui_scrStart_screen_init(void);

extern lv_obj_t *ui_scrStart;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlMessage;
extern lv_obj_t *ui_lblMessage;
extern lv_obj_t *ui_barProgress;

LV_IMG_DECLARE(ui_img_nothing_png);

void ui_init(void);
