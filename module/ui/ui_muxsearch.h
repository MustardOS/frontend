#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_mux(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_scrSearch;

extern lv_obj_t *ui_pnlLookup;
extern lv_obj_t *ui_pnlSearchLocal;
extern lv_obj_t *ui_pnlSearchGlobal;

extern lv_obj_t *ui_lblLookup;
extern lv_obj_t *ui_lblSearchLocal;
extern lv_obj_t *ui_lblSearchGlobal;

extern lv_obj_t *ui_icoLookup;
extern lv_obj_t *ui_icoSearchLocal;
extern lv_obj_t *ui_icoSearchGlobal;

extern lv_obj_t *ui_lblLookupValue;
extern lv_obj_t *ui_lblSearchLocalValue;
extern lv_obj_t *ui_lblSearchGlobalValue;

extern lv_obj_t *ui_pnlEntry;
extern lv_obj_t *ui_txtEntry;
