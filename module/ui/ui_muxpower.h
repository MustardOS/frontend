#pragma once

#include "../../lvgl/lvgl.h"

void init_mux(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlShutdown;
extern lv_obj_t *ui_pnlBattery;
extern lv_obj_t *ui_pnlIdleDisplay;
extern lv_obj_t *ui_pnlIdleSleep;

extern lv_obj_t *ui_lblShutdown;
extern lv_obj_t *ui_lblBattery;
extern lv_obj_t *ui_lblIdleDisplay;
extern lv_obj_t *ui_lblIdleSleep;

extern lv_obj_t *ui_icoShutdown;
extern lv_obj_t *ui_icoBattery;
extern lv_obj_t *ui_icoIdleDisplay;
extern lv_obj_t *ui_icoIdleSleep;

extern lv_obj_t *ui_droShutdown;
extern lv_obj_t *ui_droBattery;
extern lv_obj_t *ui_droIdleDisplay;
extern lv_obj_t *ui_droIdleSleep;
