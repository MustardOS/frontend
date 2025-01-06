#include "ui_muxlaunch.h"

lv_obj_t *ui_pnlExplore;
lv_obj_t *ui_pnlCollection;
lv_obj_t *ui_pnlHistory;
lv_obj_t *ui_pnlApps;
lv_obj_t *ui_pnlInfo;
lv_obj_t *ui_pnlConfig;
lv_obj_t *ui_pnlReboot;
lv_obj_t *ui_pnlShutdown;

lv_obj_t *ui_lblContent;
lv_obj_t *ui_lblCollection;
lv_obj_t *ui_lblHistory;
lv_obj_t *ui_lblApps;
lv_obj_t *ui_lblInfo;
lv_obj_t *ui_lblConfig;
lv_obj_t *ui_lblReboot;
lv_obj_t *ui_lblShutdown;

lv_obj_t *ui_icoContent;
lv_obj_t *ui_icoCollection;
lv_obj_t *ui_icoHistory;
lv_obj_t *ui_icoApps;
lv_obj_t *ui_icoInfo;
lv_obj_t *ui_icoConfig;
lv_obj_t *ui_icoReboot;
lv_obj_t *ui_icoShutdown;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_pnlExplore = lv_obj_create(ui_pnlContent);
    ui_pnlCollection = lv_obj_create(ui_pnlContent);
    ui_pnlHistory = lv_obj_create(ui_pnlContent);
    ui_pnlApps = lv_obj_create(ui_pnlContent);
    ui_pnlInfo = lv_obj_create(ui_pnlContent);
    ui_pnlConfig = lv_obj_create(ui_pnlContent);
    ui_pnlReboot = lv_obj_create(ui_pnlContent);
    ui_pnlShutdown = lv_obj_create(ui_pnlContent);

    ui_lblContent = lv_label_create(ui_pnlExplore);
    ui_lblCollection = lv_label_create(ui_pnlCollection);
    ui_lblHistory = lv_label_create(ui_pnlHistory);
    ui_lblApps = lv_label_create(ui_pnlApps);
    ui_lblInfo = lv_label_create(ui_pnlInfo);
    ui_lblConfig = lv_label_create(ui_pnlConfig);
    ui_lblReboot = lv_label_create(ui_pnlReboot);
    ui_lblShutdown = lv_label_create(ui_pnlShutdown);

    lv_label_set_text(ui_lblContent, "");
    lv_label_set_text(ui_lblCollection, "");
    lv_label_set_text(ui_lblHistory, "");
    lv_label_set_text(ui_lblApps, "");
    lv_label_set_text(ui_lblInfo, "");
    lv_label_set_text(ui_lblConfig, "");
    lv_label_set_text(ui_lblReboot, "");
    lv_label_set_text(ui_lblShutdown, "");

    ui_icoContent = lv_img_create(ui_pnlExplore);
    ui_icoCollection = lv_img_create(ui_pnlCollection);
    ui_icoHistory = lv_img_create(ui_pnlHistory);
    ui_icoApps = lv_img_create(ui_pnlApps);
    ui_icoInfo = lv_img_create(ui_pnlInfo);
    ui_icoConfig = lv_img_create(ui_pnlConfig);
    ui_icoReboot = lv_img_create(ui_pnlReboot);
    ui_icoShutdown = lv_img_create(ui_pnlShutdown);
}
