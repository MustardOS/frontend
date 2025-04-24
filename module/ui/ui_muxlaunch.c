#include "ui_muxlaunch.h"

lv_obj_t *ui_pnlExplore_launch;
lv_obj_t *ui_pnlCollection_launch;
lv_obj_t *ui_pnlHistory_launch;
lv_obj_t *ui_pnlApps_launch;
lv_obj_t *ui_pnlInfo_launch;
lv_obj_t *ui_pnlConfig_launch;
lv_obj_t *ui_pnlReboot_launch;
lv_obj_t *ui_pnlShutdown_launch;

lv_obj_t *ui_lblContent_launch;
lv_obj_t *ui_lblCollection_launch;
lv_obj_t *ui_lblHistory_launch;
lv_obj_t *ui_lblApps_launch;
lv_obj_t *ui_lblInfo_launch;
lv_obj_t *ui_lblConfig_launch;
lv_obj_t *ui_lblReboot_launch;
lv_obj_t *ui_lblShutdown_launch;

lv_obj_t *ui_icoContent_launch;
lv_obj_t *ui_icoCollection_launch;
lv_obj_t *ui_icoHistory_launch;
lv_obj_t *ui_icoApps_launch;
lv_obj_t *ui_icoInfo_launch;
lv_obj_t *ui_icoConfig_launch;
lv_obj_t *ui_icoReboot_launch;
lv_obj_t *ui_icoShutdown_launch;

void init_muxlaunch(lv_obj_t *ui_pnlContent) {
    ui_pnlExplore_launch = lv_obj_create(ui_pnlContent);
    ui_pnlCollection_launch = lv_obj_create(ui_pnlContent);
    ui_pnlHistory_launch = lv_obj_create(ui_pnlContent);
    ui_pnlApps_launch = lv_obj_create(ui_pnlContent);
    ui_pnlInfo_launch = lv_obj_create(ui_pnlContent);
    ui_pnlConfig_launch = lv_obj_create(ui_pnlContent);
    ui_pnlReboot_launch = lv_obj_create(ui_pnlContent);
    ui_pnlShutdown_launch = lv_obj_create(ui_pnlContent);

    ui_lblContent_launch = lv_label_create(ui_pnlExplore_launch);
    lv_label_set_text(ui_lblContent_launch, "");
    ui_lblCollection_launch = lv_label_create(ui_pnlCollection_launch);
    lv_label_set_text(ui_lblCollection_launch, "");
    ui_lblHistory_launch = lv_label_create(ui_pnlHistory_launch);
    lv_label_set_text(ui_lblHistory_launch, "");
    ui_lblApps_launch = lv_label_create(ui_pnlApps_launch);
    lv_label_set_text(ui_lblApps_launch, "");
    ui_lblInfo_launch = lv_label_create(ui_pnlInfo_launch);
    lv_label_set_text(ui_lblInfo_launch, "");
    ui_lblConfig_launch = lv_label_create(ui_pnlConfig_launch);
    lv_label_set_text(ui_lblConfig_launch, "");
    ui_lblReboot_launch = lv_label_create(ui_pnlReboot_launch);
    lv_label_set_text(ui_lblReboot_launch, "");
    ui_lblShutdown_launch = lv_label_create(ui_pnlShutdown_launch);
    lv_label_set_text(ui_lblShutdown_launch, "");

    lv_label_set_text(ui_lblContent_launch, "");
    lv_label_set_text(ui_lblCollection_launch, "");
    lv_label_set_text(ui_lblHistory_launch, "");
    lv_label_set_text(ui_lblApps_launch, "");
    lv_label_set_text(ui_lblInfo_launch, "");
    lv_label_set_text(ui_lblConfig_launch, "");
    lv_label_set_text(ui_lblReboot_launch, "");
    lv_label_set_text(ui_lblShutdown_launch, "");

    ui_icoContent_launch = lv_img_create(ui_pnlExplore_launch);
    ui_icoCollection_launch = lv_img_create(ui_pnlCollection_launch);
    ui_icoHistory_launch = lv_img_create(ui_pnlHistory_launch);
    ui_icoApps_launch = lv_img_create(ui_pnlApps_launch);
    ui_icoInfo_launch = lv_img_create(ui_pnlInfo_launch);
    ui_icoConfig_launch = lv_img_create(ui_pnlConfig_launch);
    ui_icoReboot_launch = lv_img_create(ui_pnlReboot_launch);
    ui_icoShutdown_launch = lv_img_create(ui_pnlShutdown_launch);
}
