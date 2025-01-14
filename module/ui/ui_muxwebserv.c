#include "ui_muxwebserv.h"

lv_obj_t *ui_pnlSSHD;
lv_obj_t *ui_pnlSFTPGo;
lv_obj_t *ui_pnlTTYD;
lv_obj_t *ui_pnlSyncthing;
lv_obj_t *ui_pnlRSLSync;
lv_obj_t *ui_pnlNTP;
lv_obj_t *ui_pnlTailscaled;

lv_obj_t *ui_lblSSHD;
lv_obj_t *ui_lblSFTPGo;
lv_obj_t *ui_lblTTYD;
lv_obj_t *ui_lblSyncthing;
lv_obj_t *ui_lblRSLSync;
lv_obj_t *ui_lblNTP;
lv_obj_t *ui_lblTailscaled;

lv_obj_t *ui_icoSSHD;
lv_obj_t *ui_icoSFTPGo;
lv_obj_t *ui_icoTTYD;
lv_obj_t *ui_icoSyncthing;
lv_obj_t *ui_icoRSLSync;
lv_obj_t *ui_icoNTP;
lv_obj_t *ui_icoTailscaled;

lv_obj_t *ui_droSSHD;
lv_obj_t *ui_droSFTPGo;
lv_obj_t *ui_droTTYD;
lv_obj_t *ui_droSyncthing;
lv_obj_t *ui_droRSLSync;
lv_obj_t *ui_droNTP;
lv_obj_t *ui_droTailscaled;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlSSHD = lv_obj_create(ui_pnlContent);
    ui_pnlSFTPGo = lv_obj_create(ui_pnlContent);
    ui_pnlTTYD = lv_obj_create(ui_pnlContent);
    ui_pnlSyncthing = lv_obj_create(ui_pnlContent);
    ui_pnlRSLSync = lv_obj_create(ui_pnlContent);
    ui_pnlNTP = lv_obj_create(ui_pnlContent);
    ui_pnlTailscaled = lv_obj_create(ui_pnlContent);

    ui_lblSSHD = lv_label_create(ui_pnlSSHD);
    ui_lblSFTPGo = lv_label_create(ui_pnlSFTPGo);
    ui_lblTTYD = lv_label_create(ui_pnlTTYD);
    ui_lblSyncthing = lv_label_create(ui_pnlSyncthing);
    ui_lblRSLSync = lv_label_create(ui_pnlRSLSync);
    ui_lblNTP = lv_label_create(ui_pnlNTP);
    ui_lblTailscaled = lv_label_create(ui_pnlTailscaled);

    ui_icoSSHD = lv_img_create(ui_pnlSSHD);
    ui_icoSFTPGo = lv_img_create(ui_pnlSFTPGo);
    ui_icoTTYD = lv_img_create(ui_pnlTTYD);
    ui_icoSyncthing = lv_img_create(ui_pnlSyncthing);
    ui_icoRSLSync = lv_img_create(ui_pnlRSLSync);
    ui_icoNTP = lv_img_create(ui_pnlNTP);
    ui_icoTailscaled = lv_img_create(ui_pnlTailscaled);

    ui_droSSHD = lv_dropdown_create(ui_pnlSSHD);
    ui_droSFTPGo = lv_dropdown_create(ui_pnlSFTPGo);
    ui_droTTYD = lv_dropdown_create(ui_pnlTTYD);
    ui_droSyncthing = lv_dropdown_create(ui_pnlSyncthing);
    ui_droRSLSync = lv_dropdown_create(ui_pnlRSLSync);
    ui_droNTP = lv_dropdown_create(ui_pnlNTP);
    ui_droTailscaled = lv_dropdown_create(ui_pnlTailscaled);
}
