#include "ui_muxshare.h"
#include "ui_muxwebcode.h"

#include <common/display/theme.h>
#include <common/platform/device.h>

lv_obj_t *ui_lbl_code_webcode;
lv_obj_t *ui_pnl_expiry_webcode;
lv_obj_t *ui_bar_expiry_webcode;
lv_obj_t *ui_lbl_expiry_webcode;
lv_obj_t *ui_lbl_address_webcode;
lv_obj_t *ui_lbl_notice_webcode;

static lv_obj_t *make_label(lv_obj_t *parent, const int alpha) {
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, "");
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(label, lv_color_hex(theme.list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    return label;
}

void init_muxwebcode(lv_obj_t *ui_pnl_content, const lv_font_t *code_font) {
    lv_obj_set_flex_flow(ui_pnl_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_pnl_content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(ui_pnl_content, 16, MU_OBJ_MAIN_DEFAULT);

    /* The code itself, spaced out so it can be read off the screen at arm's length. */
    ui_lbl_code_webcode = make_label(ui_pnl_content, theme.list_default.text_alpha);
    lv_obj_set_style_text_letter_space(ui_lbl_code_webcode, device.mux.width >= 1280 ? 16 : 10, MU_OBJ_MAIN_DEFAULT);
    if (code_font) lv_obj_set_style_text_font(ui_lbl_code_webcode, code_font, MU_OBJ_MAIN_DEFAULT);

    /* Drains over the life of the code, so it is obvious when to wait for the next one.
       Coloured like the storage meters so it reads as the same kind of bar. */
    ui_pnl_expiry_webcode = lv_obj_create(ui_pnl_content);
    lv_obj_set_width(ui_pnl_expiry_webcode, lv_pct(60));
    lv_obj_set_height(ui_pnl_expiry_webcode, 10);
    lv_obj_clear_flag(ui_pnl_expiry_webcode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_pnl_expiry_webcode, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_expiry_webcode, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_pnl_expiry_webcode, 0, MU_OBJ_MAIN_DEFAULT);

    ui_bar_expiry_webcode = lv_bar_create(ui_pnl_expiry_webcode);
    lv_obj_set_width(ui_bar_expiry_webcode, lv_pct(100));
    lv_obj_set_height(ui_bar_expiry_webcode, 10);
    lv_obj_set_align(ui_bar_expiry_webcode, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_bar_expiry_webcode, lv_color_hex(theme.verbose_boot.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_expiry_webcode, 25, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_bar_expiry_webcode, lv_color_hex(theme.verbose_boot.text), MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_expiry_webcode, LV_OPA_COVER, MU_OBJ_INDI_DEFAULT);

    ui_lbl_expiry_webcode = make_label(ui_pnl_content, theme.list_default.text_alpha / 2);
    ui_lbl_address_webcode = make_label(ui_pnl_content, theme.list_default.text_alpha);
    ui_lbl_notice_webcode = make_label(ui_pnl_content, theme.list_default.text_alpha);
}
