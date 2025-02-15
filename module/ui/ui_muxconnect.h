#pragma once

#include "../../lvgl/lvgl.h"

void init_mux(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlNetwork;
extern lv_obj_t *ui_pnlServices;
extern lv_obj_t *ui_pnlBluetooth;
extern lv_obj_t *ui_pnlUSBFunction;

extern lv_obj_t *ui_lblNetwork;
extern lv_obj_t *ui_lblServices;
extern lv_obj_t *ui_lblBluetooth;
extern lv_obj_t *ui_lblUSBFunction;

extern lv_obj_t *ui_icoNetwork;
extern lv_obj_t *ui_icoServices;
extern lv_obj_t *ui_icoBluetooth;
extern lv_obj_t *ui_icoUSBFunction;

extern lv_obj_t *ui_droNetwork;
extern lv_obj_t *ui_droServices;
extern lv_obj_t *ui_droBluetooth;
extern lv_obj_t *ui_droUSBFunction;
