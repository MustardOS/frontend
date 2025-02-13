#include "ui_muxconfig.h"

lv_obj_t *ui_pnlServices;
lv_obj_t *ui_pnlNetwork;
lv_obj_t *ui_pnlUSBFunction;

lv_obj_t *ui_lblServices;
lv_obj_t *ui_lblNetwork;
lv_obj_t *ui_lblUSBFunction;

lv_obj_t *ui_icoServices;
lv_obj_t *ui_icoNetwork;
lv_obj_t *ui_icoUSBFunction;

lv_obj_t *ui_droServices;
lv_obj_t *ui_droNetwork;
lv_obj_t *ui_droUSBFunction;

void init_mux(lv_obj_t *ui_pnlContent) {
    ui_pnlServices = lv_obj_create(ui_pnlContent);
    ui_pnlUSBFunction = lv_obj_create(ui_pnlContent);
    ui_pnlNetwork = lv_obj_create(ui_pnlContent);

    ui_lblServices = lv_label_create(ui_pnlServices);
    ui_lblUSBFunction = lv_label_create(ui_pnlUSBFunction);
    ui_lblNetwork = lv_label_create(ui_pnlNetwork);

    ui_icoServices = lv_img_create(ui_pnlServices);
    ui_icoUSBFunction = lv_img_create(ui_pnlUSBFunction);
    ui_icoNetwork = lv_img_create(ui_pnlNetwork);

    ui_droServices = lv_dropdown_create(ui_pnlServices);
    ui_droUSBFunction = lv_dropdown_create(ui_pnlUSBFunction);
    ui_droNetwork = lv_dropdown_create(ui_pnlNetwork);
}
