#include "ui.h"

void ui_scrSplash_screen_init(void);

lv_obj_t *ui_scrSplash;

void ui_init(void) {
    ui_scrSplash_screen_init();
    lv_disp_load_scr(ui_scrSplash);
}
