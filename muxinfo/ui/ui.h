#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_scrInfo;
extern lv_obj_t *ui_pnlTracker;
extern lv_obj_t *ui_pnlTester;
extern lv_obj_t *ui_pnlSystem;
extern lv_obj_t *ui_pnlCredits;
extern lv_obj_t *ui_lblTracker;
extern lv_obj_t *ui_lblTester;
extern lv_obj_t *ui_lblSystem;
extern lv_obj_t *ui_lblCredits;
extern lv_obj_t *ui_icoTracker;
extern lv_obj_t *ui_icoTester;
extern lv_obj_t *ui_icoSystem;
extern lv_obj_t *ui_icoCredits;

void ui_init(lv_obj_t *ui_pnlContent);
