#pragma once

#include "../../lvgl/lvgl.h"

void ui_scrCharge_screen_init(void);

extern lv_obj_t *ui_scrCharge;
extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_imgWall;
extern lv_obj_t *ui_pnlCharge;
extern lv_obj_t *ui_lblCapacity;
extern lv_obj_t *ui_lblVoltage;
extern lv_obj_t *ui_lblHealth;
extern lv_obj_t *ui_lblBoot;

LV_IMG_DECLARE(ui_img_nothing_png);

void ui_init(void);

