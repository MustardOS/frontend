#include "ui_muxconfig.h"

lv_obj_t *ui_pnlTweakGeneral;
lv_obj_t *ui_pnlConnect;
lv_obj_t *ui_pnlCustom;
lv_obj_t *ui_pnlInterface;
lv_obj_t *ui_pnlLanguage;
lv_obj_t *ui_pnlPower;
lv_obj_t *ui_pnlStorage;
lv_obj_t *ui_pnlKiosk;

lv_obj_t *ui_lblTweakGeneral;
lv_obj_t *ui_lblConnect;
lv_obj_t *ui_lblCustom;
lv_obj_t *ui_lblInterface;
lv_obj_t *ui_lblLanguage;
lv_obj_t *ui_lblPower;
lv_obj_t *ui_lblStorage;
lv_obj_t *ui_lblKiosk;

lv_obj_t *ui_icoTweakGeneral;
lv_obj_t *ui_icoConnect;
lv_obj_t *ui_icoCustom;
lv_obj_t *ui_icoInterface;
lv_obj_t *ui_icoLanguage;
lv_obj_t *ui_icoPower;
lv_obj_t *ui_icoStorage;
lv_obj_t *ui_icoKiosk;

void init_muxconfig(lv_obj_t *ui_pnlContent) {
    ui_pnlTweakGeneral = lv_obj_create(ui_pnlContent);
    ui_pnlConnect = lv_obj_create(ui_pnlContent);
    ui_pnlCustom = lv_obj_create(ui_pnlContent);
    ui_pnlInterface = lv_obj_create(ui_pnlContent);
    ui_pnlLanguage = lv_obj_create(ui_pnlContent);
    ui_pnlPower = lv_obj_create(ui_pnlContent);
    ui_pnlStorage = lv_obj_create(ui_pnlContent);
    ui_pnlKiosk = lv_obj_create(ui_pnlContent);

    ui_lblTweakGeneral = lv_label_create(ui_pnlTweakGeneral);
    lv_label_set_text(ui_lblTweakGeneral, "");
    ui_lblConnect = lv_label_create(ui_pnlConnect);
    lv_label_set_text(ui_lblConnect, "");
    ui_lblCustom = lv_label_create(ui_pnlCustom);
    lv_label_set_text(ui_lblCustom, "");
    ui_lblInterface = lv_label_create(ui_pnlInterface);
    lv_label_set_text(ui_lblInterface, "");
    ui_lblLanguage = lv_label_create(ui_pnlLanguage);
    lv_label_set_text(ui_lblLanguage, "");
    ui_lblPower = lv_label_create(ui_pnlPower);
    lv_label_set_text(ui_lblPower, "");
    ui_lblStorage = lv_label_create(ui_pnlStorage);
    lv_label_set_text(ui_lblStorage, "");
    ui_lblKiosk = lv_label_create(ui_pnlKiosk);
    lv_label_set_text(ui_lblKiosk, "");

    ui_icoTweakGeneral = lv_img_create(ui_pnlTweakGeneral);
    ui_icoConnect = lv_img_create(ui_pnlConnect);
    ui_icoCustom = lv_img_create(ui_pnlCustom);
    ui_icoInterface = lv_img_create(ui_pnlInterface);
    ui_icoLanguage = lv_img_create(ui_pnlLanguage);
    ui_icoPower = lv_img_create(ui_pnlPower);
    ui_icoStorage = lv_img_create(ui_pnlStorage);
    ui_icoKiosk = lv_img_create(ui_pnlKiosk);
}
