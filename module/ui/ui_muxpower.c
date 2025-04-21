#include "ui_muxpower.h"

lv_obj_t *ui_pnlShutdown;
lv_obj_t *ui_pnlBattery;
lv_obj_t *ui_pnlIdleDisplay;
lv_obj_t *ui_pnlIdleSleep;

lv_obj_t *ui_lblShutdown;
lv_obj_t *ui_lblBattery;
lv_obj_t *ui_lblIdleDisplay;
lv_obj_t *ui_lblIdleSleep;

lv_obj_t *ui_icoShutdown;
lv_obj_t *ui_icoBattery;
lv_obj_t *ui_icoIdleDisplay;
lv_obj_t *ui_icoIdleSleep;

lv_obj_t *ui_droShutdown;
lv_obj_t *ui_droBattery_power;
lv_obj_t *ui_droIdleDisplay;
lv_obj_t *ui_droIdleSleep;

void init_muxpower(lv_obj_t *ui_pnlContent) {
    ui_pnlBattery = lv_obj_create(ui_pnlContent);
    ui_pnlIdleDisplay = lv_obj_create(ui_pnlContent);
    ui_pnlIdleSleep = lv_obj_create(ui_pnlContent);
    ui_pnlShutdown = lv_obj_create(ui_pnlContent);

    ui_lblShutdown = lv_label_create(ui_pnlShutdown);
    lv_label_set_text(ui_lblShutdown, "");
    ui_lblBattery = lv_label_create(ui_pnlBattery);
    lv_label_set_text(ui_lblBattery, "");
    ui_lblIdleDisplay = lv_label_create(ui_pnlIdleDisplay);
    lv_label_set_text(ui_lblIdleDisplay, "");
    ui_lblIdleSleep = lv_label_create(ui_pnlIdleSleep);
    lv_label_set_text(ui_lblIdleSleep, "");

    ui_icoShutdown = lv_img_create(ui_pnlShutdown);
    ui_icoBattery = lv_img_create(ui_pnlBattery);
    ui_icoIdleDisplay = lv_img_create(ui_pnlIdleDisplay);
    ui_icoIdleSleep = lv_img_create(ui_pnlIdleSleep);

    ui_droShutdown = lv_dropdown_create(ui_pnlShutdown);
    lv_dropdown_clear_options(ui_droShutdown);
    lv_obj_set_style_text_opa(ui_droShutdown, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droBattery_power = lv_dropdown_create(ui_pnlBattery);
    lv_dropdown_clear_options(ui_droBattery_power);
    lv_obj_set_style_text_opa(ui_droBattery_power, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droIdleDisplay = lv_dropdown_create(ui_pnlIdleDisplay);
    lv_dropdown_clear_options(ui_droIdleDisplay);
    lv_obj_set_style_text_opa(ui_droIdleDisplay, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    ui_droIdleSleep = lv_dropdown_create(ui_pnlIdleSleep);
    lv_dropdown_clear_options(ui_droIdleSleep);
    lv_obj_set_style_text_opa(ui_droIdleSleep, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
}
