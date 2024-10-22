#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlTweakGeneral;
extern lv_obj_t *ui_pnlTheme;
extern lv_obj_t *ui_pnlNetwork;
extern lv_obj_t *ui_pnlServices;
extern lv_obj_t *ui_pnlRTC;
extern lv_obj_t *ui_pnlLanguage;
extern lv_obj_t *ui_pnlStorage;
extern lv_obj_t *ui_lblTweakGeneral;
extern lv_obj_t *ui_lblTheme;
extern lv_obj_t *ui_lblNetwork;
extern lv_obj_t *ui_lblServices;
extern lv_obj_t *ui_lblRTC;
extern lv_obj_t *ui_lblLanguage;
extern lv_obj_t *ui_lblStorage;
extern lv_obj_t *ui_icoTweakGeneral;
extern lv_obj_t *ui_icoTheme;
extern lv_obj_t *ui_icoNetwork;
extern lv_obj_t *ui_icoServices;
extern lv_obj_t *ui_icoRTC;
extern lv_obj_t *ui_icoLanguage;
extern lv_obj_t *ui_icoStorage;

void ui_init(lv_obj_t *ui_pnlContent);
