#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlCatalogue = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);

    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblCatalogue = lv_label_create(ui_pnlCatalogue);
    ui_lblConfig = lv_label_create(ui_pnlConfig);

    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoCatalogue = lv_img_create(ui_pnlCatalogue);
    ui_icoConfig = lv_img_create(ui_pnlConfig);
}
