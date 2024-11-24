#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlTweakGeneral = lv_obj_create(ui_pnlContent);
    ui_pnlCustom = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);
    ui_pnlServices = lv_obj_create(ui_pnlContent);
    ui_pnlRTC = lv_obj_create(ui_pnlContent);
    ui_pnlLanguage = lv_obj_create(ui_pnlContent);
    ui_pnlStorage = lv_obj_create(ui_pnlContent);

    ui_lblTweakGeneral = lv_label_create(ui_pnlTweakGeneral);
    ui_lblCustom = lv_label_create(ui_pnlCustom);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);
    ui_lblServices = lv_label_create(ui_pnlServices);
    ui_lblRTC = lv_label_create(ui_pnlRTC);
    ui_lblLanguage = lv_label_create(ui_pnlLanguage);
    ui_lblStorage = lv_label_create(ui_pnlStorage);

    ui_icoTweakGeneral = lv_img_create(ui_pnlTweakGeneral);
    ui_icoCustom = lv_img_create(ui_pnlCustom);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);
    ui_icoServices = lv_img_create(ui_pnlServices);
    ui_icoRTC = lv_img_create(ui_pnlRTC);
    ui_icoLanguage = lv_img_create(ui_pnlLanguage);
    ui_icoStorage = lv_img_create(ui_pnlStorage);
}
