#pragma once

#include "../../lvgl/lvgl.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

extern lv_obj_t *ui_pnlTheme;
extern lv_obj_t *ui_pnlCatalogue;
extern lv_obj_t *ui_pnlConfig;

extern lv_obj_t *ui_lblTheme;
extern lv_obj_t *ui_lblCatalogue;
extern lv_obj_t *ui_lblConfig;

extern lv_obj_t *ui_icoTheme;
extern lv_obj_t *ui_icoCatalogue;
extern lv_obj_t *ui_icoConfig;

void ui_init(lv_obj_t *ui_pnlContent);
