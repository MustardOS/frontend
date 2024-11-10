#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlSSHD = lv_obj_create(ui_pnlContent);
    ui_pnlSFTPGo = lv_obj_create(ui_pnlContent);
    ui_pnlTTYD = lv_obj_create(ui_pnlContent);
    ui_pnlSyncthing = lv_obj_create(ui_pnlContent);
    ui_pnlRSLSync = lv_obj_create(ui_pnlContent);
    ui_pnlNTP = lv_obj_create(ui_pnlContent);

    ui_lblSSHD = lv_label_create(ui_pnlSSHD);
    ui_lblSFTPGo = lv_label_create(ui_pnlSFTPGo);
    ui_lblTTYD = lv_label_create(ui_pnlTTYD);
    ui_lblSyncthing = lv_label_create(ui_pnlSyncthing);
    ui_lblRSLSync = lv_label_create(ui_pnlRSLSync);
    ui_lblNTP = lv_label_create(ui_pnlNTP);

    ui_icoSSHD = lv_img_create(ui_pnlSSHD);
    ui_icoSFTPGo = lv_img_create(ui_pnlSFTPGo);
    ui_icoTTYD = lv_img_create(ui_pnlTTYD);
    ui_icoSyncthing = lv_img_create(ui_pnlSyncthing);
    ui_icoRSLSync = lv_img_create(ui_pnlRSLSync);
    ui_icoNTP = lv_img_create(ui_pnlNTP);

    ui_droSSHD = lv_dropdown_create(ui_pnlSSHD);
    ui_droSFTPGo = lv_dropdown_create(ui_pnlSFTPGo);
    ui_droTTYD = lv_dropdown_create(ui_pnlTTYD);
    ui_droSyncthing = lv_dropdown_create(ui_pnlSyncthing);
    ui_droRSLSync = lv_dropdown_create(ui_pnlRSLSync);
    ui_droNTP = lv_dropdown_create(ui_pnlNTP);
}
