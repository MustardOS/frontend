#pragma once

#include "../../lvgl/lvgl.h"
#include "../../common/theme.h"

void init_muxsearch(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

extern lv_obj_t *ui_scrSearch_search;

extern lv_obj_t *ui_pnlLookup_search;
extern lv_obj_t *ui_pnlSearchLocal_search;
extern lv_obj_t *ui_pnlSearchGlobal_search;

extern lv_obj_t *ui_lblLookup_search;
extern lv_obj_t *ui_lblSearchLocal_search;
extern lv_obj_t *ui_lblSearchGlobal_search;

extern lv_obj_t *ui_icoLookup_search;
extern lv_obj_t *ui_icoSearchLocal_search;
extern lv_obj_t *ui_icoSearchGlobal_search;

extern lv_obj_t *ui_lblLookupValue_search;
extern lv_obj_t *ui_lblSearchLocalValue_search;
extern lv_obj_t *ui_lblSearchGlobalValue_search;

extern lv_obj_t *ui_pnlEntry_search;
extern lv_obj_t *ui_txtEntry_search;
