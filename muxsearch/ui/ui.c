#include "ui.h"

void ui_screen_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme);

lv_obj_t *ui_scrSearch;

lv_obj_t *ui_pnlLookup;
lv_obj_t *ui_pnlSearch;

lv_obj_t *ui_lblLookup;
lv_obj_t *ui_lblSearch;

lv_obj_t *ui_icoLookup;
lv_obj_t *ui_icoSearch;

lv_obj_t *ui_lblLookupValue;
lv_obj_t *ui_lblSearchValue;

lv_obj_t *ui_pnlEntry;
lv_obj_t *ui_txtEntry;

void ui_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_screen_init(ui_screen, ui_pnlContent, theme);
}
