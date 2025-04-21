#pragma once

#include "../../lvgl/lvgl.h"

void init_muxcharge(void);

extern lv_obj_t *ui_scrCharge;
extern lv_obj_t *ui_blank;

extern lv_obj_t *ui_imgWall;

extern lv_obj_t *ui_pnlWall;
extern lv_obj_t *ui_pnlCharge;

extern lv_obj_t *ui_lblCapacity;
extern lv_obj_t *ui_lblVoltage;
extern lv_obj_t *ui_lblBoot;
