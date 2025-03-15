#include "ui_muxconfig.h"

lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_pnlServices;
lv_obj_t *ui_pnlBluetooth;
lv_obj_t *ui_pnlUSBFunction;

lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_lblServices;
lv_obj_t *ui_lblBluetooth;
lv_obj_t *ui_lblUSBFunction;

lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_icoServices;
lv_obj_t *ui_icoBluetooth;
lv_obj_t *ui_icoUSBFunction;

lv_obj_t *ui_droNetwork;
lv_obj_t *ui_droServices;
lv_obj_t *ui_droBluetooth;
lv_obj_t *ui_droUSBFunction;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);
    ui_pnlServices = lv_obj_create(ui_pnlContent);
    ui_pnlBluetooth = lv_obj_create(ui_pnlContent);
    ui_pnlUSBFunction = lv_obj_create(ui_pnlContent);

    ui_lblNetwork = lv_label_create(ui_pnlNetwork);
    lv_label_set_text(ui_lblNetwork, "");
    ui_lblServices = lv_label_create(ui_pnlServices);
    lv_label_set_text(ui_lblServices, "");
    ui_lblBluetooth = lv_label_create(ui_pnlBluetooth);
    lv_label_set_text(ui_lblBluetooth, "");
    ui_lblUSBFunction = lv_label_create(ui_pnlUSBFunction);
    lv_label_set_text(ui_lblUSBFunction, "");

    ui_icoNetwork = lv_img_create(ui_pnlNetwork);
    ui_icoServices = lv_img_create(ui_pnlServices);
    ui_icoBluetooth = lv_img_create(ui_pnlBluetooth);
    ui_icoUSBFunction = lv_img_create(ui_pnlUSBFunction);

    ui_droNetwork = lv_dropdown_create(ui_pnlNetwork);
    lv_dropdown_clear_options(ui_droNetwork);
    lv_obj_set_style_text_opa(ui_droNetwork, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droServices = lv_dropdown_create(ui_pnlServices);
    lv_dropdown_clear_options(ui_droServices);
    lv_obj_set_style_text_opa(ui_droServices, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droBluetooth = lv_dropdown_create(ui_pnlBluetooth);
    lv_dropdown_clear_options(ui_droBluetooth);
    lv_obj_set_style_text_opa(ui_droBluetooth, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droUSBFunction = lv_dropdown_create(ui_pnlUSBFunction);
    lv_dropdown_clear_options(ui_droUSBFunction);
    lv_obj_set_style_text_opa(ui_droUSBFunction, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
