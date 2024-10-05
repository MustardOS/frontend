#include "ui.h"

void ui_screen_init(lv_obj_t *ui_pnlContent);

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
lv_obj_t *ui_droBattery;
lv_obj_t *ui_droIdleDisplay;
lv_obj_t *ui_droIdleSleep;

void ui_init(lv_obj_t *ui_pnlContent) {
    ui_screen_init(ui_pnlContent);
}
