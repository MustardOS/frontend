#include "ui.h"

void ui_scrCharge_screen_init(void);

lv_obj_t *ui_scrCharge;
lv_obj_t *ui_pnlWall;
lv_obj_t *ui_imgWall;
lv_obj_t *ui_pnlCharge;
lv_obj_t *ui_lblCapacity;
lv_obj_t *ui_lblVoltage;
lv_obj_t *ui_lblHealth;
lv_obj_t *ui_lblBoot;
lv_obj_t *ui____initial_actions0;

void ui_init(void) {
    ui_scrCharge_screen_init();
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_scrCharge);
}
