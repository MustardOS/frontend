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

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlSearch = lv_obj_create(ui_pnlContent);
    ui_pnlCore = lv_obj_create(ui_pnlContent);
    ui_pnlGovernor = lv_obj_create(ui_pnlContent);

    ui_lblSearch = lv_label_create(ui_pnlSearch);
    ui_lblCore = lv_label_create(ui_pnlCore);
    ui_lblGovernor = lv_label_create(ui_pnlGovernor);

    ui_icoSearch = lv_img_create(ui_pnlSearch);
    ui_icoCore = lv_img_create(ui_pnlCore);
    ui_icoGovernor = lv_img_create(ui_pnlGovernor);
}
