#include "ui_muxtext.h"

lv_obj_t *ui_pnlDocument_text;

lv_obj_t *ui_txtDocument_text;

void init_muxtext(lv_obj_t *ui_screen, struct theme_config *theme) {
    ui_pnlDocument_text = lv_obj_create(ui_screen);
    lv_obj_set_size(ui_pnlDocument_text, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(ui_pnlDocument_text, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ui_pnlDocument_text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_pnlDocument_text, 0, 0);
    lv_obj_set_style_border_width(ui_pnlDocument_text, 0, 0);
    lv_obj_set_style_pad_all(ui_pnlDocument_text, 3, 0);

    ui_txtDocument_text = lv_textarea_create(ui_pnlDocument_text);
    lv_obj_set_size(ui_txtDocument_text, LV_PCT(100), LV_PCT(100));
    lv_textarea_set_text(ui_txtDocument_text, "Loading...");
    lv_obj_set_style_text_color(ui_txtDocument_text, lv_color_hex(theme->LIST_DEFAULT.TEXT), 0);
    lv_obj_set_style_bg_opa(ui_txtDocument_text, 0, 0);
    lv_obj_set_style_border_width(ui_txtDocument_text, 0, 0);
    lv_obj_set_style_pad_all(ui_txtDocument_text, 3, 0);
}
