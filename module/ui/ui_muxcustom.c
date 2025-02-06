#include "ui_muxcustom.h"

lv_obj_t *ui_pnlTheme;
lv_obj_t *ui_pnlThemeAlternate;
lv_obj_t *ui_pnlCatalogue;
lv_obj_t *ui_pnlConfig;

lv_obj_t *ui_lblTheme;
lv_obj_t *ui_lblThemeAlternate;
lv_obj_t *ui_lblCatalogue;
lv_obj_t *ui_lblConfig;

lv_obj_t *ui_icoTheme;
lv_obj_t *ui_icoThemeAlternate;
lv_obj_t *ui_icoCatalogue;
lv_obj_t *ui_icoConfig;

lv_obj_t *ui_droTheme;
lv_obj_t *ui_droThemeAlternate;
lv_obj_t *ui_droCatalogue;
lv_obj_t *ui_droConfig;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlTheme = lv_obj_create(ui_pnlContent);
    ui_pnlThemeAlternate = lv_obj_create(ui_pnlContent);
    ui_pnlCatalogue = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);

    ui_lblTheme = lv_label_create(ui_pnlTheme);
    ui_lblThemeAlternate = lv_label_create(ui_pnlThemeAlternate);
    ui_lblCatalogue = lv_label_create(ui_pnlCatalogue);
    ui_lblConfig = lv_label_create(ui_pnlConfig);

    ui_icoTheme = lv_img_create(ui_pnlTheme);
    ui_icoThemeAlternate = lv_img_create(ui_pnlThemeAlternate);
    ui_icoCatalogue = lv_img_create(ui_pnlCatalogue);
    ui_icoConfig = lv_img_create(ui_pnlConfig);

    ui_droTheme = lv_dropdown_create(ui_pnlTheme);
    ui_droThemeAlternate = lv_dropdown_create(ui_pnlThemeAlternate);
    ui_droCatalogue = lv_dropdown_create(ui_pnlCatalogue);
    ui_droConfig = lv_dropdown_create(ui_pnlConfig);
}
