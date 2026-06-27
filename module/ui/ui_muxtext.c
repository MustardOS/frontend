#include "ui_muxtext.h"

lv_obj_t *ui_pnl_document_text;

lv_obj_t *ui_txt_document_text;

void init_muxtext(lv_obj_t *ui_screen, const struct theme_config *theme) {
    ui_pnl_document_text = lv_obj_create(ui_screen);
    lv_obj_set_size(ui_pnl_document_text, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(ui_pnl_document_text, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ui_pnl_document_text, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_pnl_document_text, 0, 0);
    lv_obj_set_style_border_width(ui_pnl_document_text, 0, 0);
    lv_obj_set_style_pad_all(ui_pnl_document_text, 3, 0);

    ui_txt_document_text = lv_textarea_create(ui_pnl_document_text);
    lv_obj_set_size(ui_txt_document_text, LV_PCT(100), LV_PCT(100));
    lv_textarea_set_text(ui_txt_document_text, "Loading...");
    lv_obj_set_style_text_color(ui_txt_document_text, lv_color_hex(theme->list_default.text), 0);
    lv_obj_set_style_bg_opa(ui_txt_document_text, 0, 0);
    lv_obj_set_style_border_width(ui_txt_document_text, 0, 0);
    lv_obj_set_style_pad_all(ui_txt_document_text, 3, 0);
}
