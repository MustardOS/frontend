#include "ui_muxwebserv.h"

lv_obj_t *ui_pnlSSHD;
lv_obj_t *ui_pnlSFTPGo;
lv_obj_t *ui_pnlTTYD;
lv_obj_t *ui_pnlSyncthing_webserv;
lv_obj_t *ui_pnlRSLSync;
lv_obj_t *ui_pnlNTP;
lv_obj_t *ui_pnlTailscaled;

lv_obj_t *ui_lblSSHD;
lv_obj_t *ui_lblSFTPGo;
lv_obj_t *ui_lblTTYD;
lv_obj_t *ui_lblSyncthing_webserv;
lv_obj_t *ui_lblRSLSync;
lv_obj_t *ui_lblNTP;
lv_obj_t *ui_lblTailscaled;

lv_obj_t *ui_icoSSHD;
lv_obj_t *ui_icoSFTPGo;
lv_obj_t *ui_icoTTYD;
lv_obj_t *ui_icoSyncthing_webserv;
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

void init_muxwebserv(lv_obj_t *ui_pnlContent) {
    ui_pnlSSHD = lv_obj_create(ui_pnlContent);
    ui_pnlSFTPGo = lv_obj_create(ui_pnlContent);
    ui_pnlTTYD = lv_obj_create(ui_pnlContent);
    ui_pnlSyncthing_webserv = lv_obj_create(ui_pnlContent);
    ui_pnlRSLSync = lv_obj_create(ui_pnlContent);
    ui_pnlNTP = lv_obj_create(ui_pnlContent);
    ui_pnlTailscaled = lv_obj_create(ui_pnlContent);

    ui_lblSSHD = lv_label_create(ui_pnlSSHD);
    lv_label_set_text(ui_lblSSHD, "");
    ui_lblSFTPGo = lv_label_create(ui_pnlSFTPGo);
    lv_label_set_text(ui_lblSFTPGo, "");
    ui_lblTTYD = lv_label_create(ui_pnlTTYD);
    lv_label_set_text(ui_lblTTYD, "");
    ui_lblSyncthing_webserv = lv_label_create(ui_pnlSyncthing_webserv);
    lv_label_set_text(ui_lblSyncthing_webserv, "");
    ui_lblRSLSync = lv_label_create(ui_pnlRSLSync);
    lv_label_set_text(ui_lblRSLSync, "");
    ui_lblNTP = lv_label_create(ui_pnlNTP);
    lv_label_set_text(ui_lblNTP, "");
    ui_lblTailscaled = lv_label_create(ui_pnlTailscaled);
    lv_label_set_text(ui_lblTailscaled, "");

    ui_icoSSHD = lv_img_create(ui_pnlSSHD);
    ui_icoSFTPGo = lv_img_create(ui_pnlSFTPGo);
    ui_icoTTYD = lv_img_create(ui_pnlTTYD);
    ui_icoSyncthing_webserv = lv_img_create(ui_pnlSyncthing_webserv);
    ui_icoRSLSync = lv_img_create(ui_pnlRSLSync);
    ui_icoNTP = lv_img_create(ui_pnlNTP);
    ui_icoTailscaled = lv_img_create(ui_pnlTailscaled);

    ui_droSSHD = lv_dropdown_create(ui_pnlSSHD);
    lv_dropdown_clear_options(ui_droSSHD);
    lv_obj_set_style_text_opa(ui_droSSHD, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droSFTPGo = lv_dropdown_create(ui_pnlSFTPGo);
    lv_dropdown_clear_options(ui_droSFTPGo);
    lv_obj_set_style_text_opa(ui_droSFTPGo, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droTTYD = lv_dropdown_create(ui_pnlTTYD);
    lv_dropdown_clear_options(ui_droTTYD);
    lv_obj_set_style_text_opa(ui_droTTYD, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droSyncthing = lv_dropdown_create(ui_pnlSyncthing_webserv);
    lv_dropdown_clear_options(ui_droSyncthing);
    lv_obj_set_style_text_opa(ui_droSyncthing, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droRSLSync = lv_dropdown_create(ui_pnlRSLSync);
    lv_dropdown_clear_options(ui_droRSLSync);
    lv_obj_set_style_text_opa(ui_droRSLSync, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droNTP = lv_dropdown_create(ui_pnlNTP);
    lv_dropdown_clear_options(ui_droNTP);
    lv_obj_set_style_text_opa(ui_droNTP, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droTailscaled = lv_dropdown_create(ui_pnlTailscaled);
    lv_dropdown_clear_options(ui_droTailscaled);
    lv_obj_set_style_text_opa(ui_droTailscaled, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
