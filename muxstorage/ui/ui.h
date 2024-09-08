#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlBIOS;
extern lv_obj_t *ui_pnlConfig;
extern lv_obj_t *ui_pnlCatalogue;
extern lv_obj_t *ui_pnlConman;
extern lv_obj_t *ui_pnlMusic;
extern lv_obj_t *ui_pnlSave;
extern lv_obj_t *ui_pnlScreenshot;
extern lv_obj_t *ui_pnlTheme;
extern lv_obj_t *ui_pnlLanguage;
extern lv_obj_t *ui_lblBIOS;
extern lv_obj_t *ui_lblConfig;
extern lv_obj_t *ui_lblCatalogue;
extern lv_obj_t *ui_lblConman;
extern lv_obj_t *ui_lblMusic;
extern lv_obj_t *ui_lblSave;
extern lv_obj_t *ui_lblScreenshot;
extern lv_obj_t *ui_lblTheme;
extern lv_obj_t *ui_lblLanguage;
extern lv_obj_t *ui_icoBIOS;
extern lv_obj_t *ui_icoConfig;
extern lv_obj_t *ui_icoCatalogue;
extern lv_obj_t *ui_icoConman;
extern lv_obj_t *ui_icoMusic;
extern lv_obj_t *ui_icoSave;
extern lv_obj_t *ui_icoScreenshot;
extern lv_obj_t *ui_icoTheme;
extern lv_obj_t *ui_icoLanguage;
extern lv_obj_t *ui_droBIOS;
extern lv_obj_t *ui_droConfig;
extern lv_obj_t *ui_droCatalogue;
extern lv_obj_t *ui_droConman;
extern lv_obj_t *ui_droMusic;
extern lv_obj_t *ui_droSave;
extern lv_obj_t *ui_droScreenshot;
extern lv_obj_t *ui_droTheme;
extern lv_obj_t *ui_droLanguage;

void ui_init(lv_obj_t *ui_pnlContent);
