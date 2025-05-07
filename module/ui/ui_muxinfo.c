#include "ui_muxinfo.h"

lv_obj_t *ui_pnlTracker;
lv_obj_t *ui_pnlScreenshot;
lv_obj_t *ui_pnlSpace;
lv_obj_t *ui_pnlTester;
lv_obj_t *ui_pnlSysinfo;
lv_obj_t *ui_pnlNetinfo;
lv_obj_t *ui_pnlCredits;

lv_obj_t *ui_lblTracker;
lv_obj_t *ui_lblScreenshot;
lv_obj_t *ui_lblSpace;
lv_obj_t *ui_lblTester;
lv_obj_t *ui_lblSysinfo;
lv_obj_t *ui_lblNetinfo;
lv_obj_t *ui_lblCredits;

lv_obj_t *ui_icoTracker;
lv_obj_t *ui_icoScreenshot;
lv_obj_t *ui_icoSpace;
lv_obj_t *ui_icoTester;
lv_obj_t *ui_icoSysinfo;
lv_obj_t *ui_icoNetinfo;
lv_obj_t *ui_icoCredits;

void init_muxinfo(lv_obj_t *ui_pnlContent) {
    ui_pnlTracker = lv_obj_create(ui_pnlContent);
    ui_pnlScreenshot = lv_obj_create(ui_pnlContent);
    ui_pnlSpace = lv_obj_create(ui_pnlContent);
    ui_pnlTester = lv_obj_create(ui_pnlContent);
    ui_pnlSysinfo = lv_obj_create(ui_pnlContent);
    ui_pnlNetinfo = lv_obj_create(ui_pnlContent);
    ui_pnlCredits = lv_obj_create(ui_pnlContent);
    lv_obj_add_flag(ui_pnlTracker, LV_OBJ_FLAG_HIDDEN);

    ui_lblTracker = lv_label_create(ui_pnlTracker);
    lv_label_set_text(ui_lblTracker, "");
    ui_lblScreenshot = lv_label_create(ui_pnlScreenshot);
    lv_label_set_text(ui_lblScreenshot, "");
    ui_lblSpace = lv_label_create(ui_pnlSpace);
    lv_label_set_text(ui_lblSpace, "");
    ui_lblTester = lv_label_create(ui_pnlTester);
    lv_label_set_text(ui_lblTester, "");
    ui_lblSysinfo = lv_label_create(ui_pnlSysinfo);
    lv_label_set_text(ui_lblSysinfo, "");
    ui_lblNetinfo = lv_label_create(ui_pnlNetinfo);
    lv_label_set_text(ui_lblNetinfo, "");
    ui_lblCredits = lv_label_create(ui_pnlCredits);
    lv_label_set_text(ui_lblCredits, "");

    ui_icoTracker = lv_img_create(ui_pnlTracker);
    ui_icoScreenshot = lv_img_create(ui_pnlScreenshot);
    ui_icoSpace = lv_img_create(ui_pnlSpace);
    ui_icoTester = lv_img_create(ui_pnlTester);
    ui_icoSysinfo = lv_img_create(ui_pnlSysinfo);
    ui_icoNetinfo = lv_img_create(ui_pnlNetinfo);
    ui_icoCredits = lv_img_create(ui_pnlCredits);
}
