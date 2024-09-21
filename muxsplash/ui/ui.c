#include "ui.h"

void ui_scrSplash_screen_init(void);

lv_obj_t *ui_scrSplash;
lv_obj_t *ui____initial_actions0;

void ui_init(void) {
    ui_scrSplash_screen_init();
    ui____initial_actions0 = lv_obj_create(NULL);
    lv_disp_load_scr(ui_scrSplash);
}
