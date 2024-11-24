#include "ui.h"

lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlCatalogue;
lv_obj_t *ui_pnlConfig;

lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblCatalogue;
lv_obj_t *ui_lblConfig;

lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoCatalogue;
lv_obj_t *ui_icoConfig;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
