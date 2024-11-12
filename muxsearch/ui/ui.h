#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void ui_screen_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_scrSearch;

extern lv_obj_t *ui_pnlLookup;
extern lv_obj_t *ui_pnlSearch;

extern lv_obj_t *ui_lblLookup;
extern lv_obj_t *ui_lblSearch;

extern lv_obj_t *ui_icoLookup;
extern lv_obj_t *ui_icoSearch;

extern lv_obj_t *ui_lblLookupValue;
extern lv_obj_t *ui_lblSearchValue;

extern lv_obj_t *ui_pnlEntry;
extern lv_obj_t *ui_txtEntry;

void ui_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);
