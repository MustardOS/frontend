#include "ui.h"

void ui_scrSplash_screen_init(void) {
    ui_scrSplash = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrSplash, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE |
                                    LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(ui_scrSplash, LV_SCROLLBAR_MODE_OFF);
}
