#include "ui_muxinfo.h"

lv_obj_t *ui_pnlTracker;
lv_obj_t *ui_pnlTester;
lv_obj_t *ui_pnlSystem;
lv_obj_t *ui_pnlCredits;

lv_obj_t *ui_lblTracker;
lv_obj_t *ui_lblTester;
lv_obj_t *ui_lblSystem;
lv_obj_t *ui_lblCredits;

lv_obj_t *ui_icoTracker;
lv_obj_t *ui_icoTester;
lv_obj_t *ui_icoSystem;
lv_obj_t *ui_icoCredits;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_pnlTracker = lv_obj_create(ui_pnlContent);
    ui_pnlTester = lv_obj_create(ui_pnlContent);
    ui_pnlSystem = lv_obj_create(ui_pnlContent);
    ui_pnlCredits = lv_obj_create(ui_pnlContent);
    lv_obj_add_flag(ui_pnlTracker, LV_OBJ_FLAG_HIDDEN);

    ui_lblTracker = lv_label_create(ui_pnlTracker);
    ui_lblTester = lv_label_create(ui_pnlTester);
    ui_lblSystem = lv_label_create(ui_pnlSystem);
    ui_lblCredits = lv_label_create(ui_pnlCredits);

    ui_icoTracker = lv_img_create(ui_pnlTracker);
    ui_icoTester = lv_img_create(ui_pnlTester);
    ui_icoSystem = lv_img_create(ui_pnlSystem);
    ui_icoCredits = lv_img_create(ui_pnlCredits);
}
