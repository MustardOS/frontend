#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent) {
    ui_pnlShutdown = lv_obj_create(ui_pnlContent);
    ui_pnlBattery = lv_obj_create(ui_pnlContent);
    ui_pnlIdleDisplay = lv_obj_create(ui_pnlContent);
    ui_pnlIdleSleep = lv_obj_create(ui_pnlContent);

    ui_lblShutdown = lv_label_create(ui_pnlShutdown);
    ui_lblBattery = lv_label_create(ui_pnlBattery);
    ui_lblIdleDisplay = lv_label_create(ui_pnlIdleDisplay);
    ui_lblIdleSleep = lv_label_create(ui_pnlIdleSleep);

    ui_icoShutdown = lv_img_create(ui_pnlShutdown);
    ui_icoBattery = lv_img_create(ui_pnlBattery);
    ui_icoIdleDisplay = lv_img_create(ui_pnlIdleDisplay);
    ui_icoIdleSleep = lv_img_create(ui_pnlIdleSleep);

    ui_droShutdown = lv_dropdown_create(ui_pnlShutdown);
    ui_droBattery = lv_dropdown_create(ui_pnlBattery);
    ui_droIdleDisplay = lv_dropdown_create(ui_pnlIdleDisplay);
    ui_droIdleSleep = lv_dropdown_create(ui_pnlIdleSleep);
}
