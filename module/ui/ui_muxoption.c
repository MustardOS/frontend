#include "ui_muxoption.h"

lv_obj_t *ui_pnlSearch;
lv_obj_t *ui_pnlCore;
lv_obj_t *ui_pnlGovernor;

lv_obj_t *ui_lblSearch;
lv_obj_t *ui_lblCore;
lv_obj_t *ui_lblGovernor;

lv_obj_t *ui_icoSearch;
lv_obj_t *ui_icoCore;
lv_obj_t *ui_icoGovernor;

void init_muxoption(lv_obj_t *ui_pnlContent) {
    ui_pnlSearch = lv_obj_create(ui_pnlContent);
    ui_pnlCore = lv_obj_create(ui_pnlContent);
    ui_pnlGovernor = lv_obj_create(ui_pnlContent);

    ui_lblSearch = lv_label_create(ui_pnlSearch);
    lv_label_set_text(ui_lblSearch, "");
    ui_lblCore = lv_label_create(ui_pnlCore);
    lv_label_set_text(ui_lblCore, "");
    ui_lblGovernor = lv_label_create(ui_pnlGovernor);
    lv_label_set_text(ui_lblGovernor, "");

    ui_icoSearch = lv_img_create(ui_pnlSearch);
    ui_icoCore = lv_img_create(ui_pnlCore);
    ui_icoGovernor = lv_img_create(ui_pnlGovernor);
}
