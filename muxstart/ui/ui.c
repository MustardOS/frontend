#include "ui.h"

void ui_scrStart_screen_init(void);

lv_obj_t *ui_scrStart;
lv_obj_t *ui_pnlWall;
lv_obj_t *ui_imgWall;
lv_obj_t *ui_pnlMessage;
lv_obj_t *ui_lblMessage;
lv_obj_t *ui____initial_actions0;

void ui_init(void) {
    ui_scrStart_screen_init();
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_scrStart);
}
