#pragma once

#include "../../lvgl/lvgl.h"

void init_mux(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlSD1;
extern lv_obj_t *ui_pnlSD2;
extern lv_obj_t *ui_pnlUSB;
extern lv_obj_t *ui_pnlRFS;

extern lv_obj_t *ui_lblSD1;
extern lv_obj_t *ui_lblSD2;
extern lv_obj_t *ui_lblUSB;
extern lv_obj_t *ui_lblRFS;

extern lv_obj_t *ui_icoSD1;
extern lv_obj_t *ui_icoSD2;
extern lv_obj_t *ui_icoUSB;
extern lv_obj_t *ui_icoRFS;

extern lv_obj_t *ui_lblSD1Value;
extern lv_obj_t *ui_lblSD2Value;
extern lv_obj_t *ui_lblUSBValue;
extern lv_obj_t *ui_lblRFSValue;

extern lv_obj_t *ui_pnlSD1Bar;
extern lv_obj_t *ui_pnlSD2Bar;
extern lv_obj_t *ui_pnlUSBBar;
extern lv_obj_t *ui_pnlRFSBar;

extern lv_obj_t *ui_barSD1;
extern lv_obj_t *ui_barSD2;
extern lv_obj_t *ui_barUSB;
extern lv_obj_t *ui_barRFS;
