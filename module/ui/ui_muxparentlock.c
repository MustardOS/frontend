#include "ui_muxparentlock.h"
#include "../../font/notosans_big.h"

lv_obj_t *ui_plrolComboOne;
lv_obj_t *ui_plrolComboTwo;
lv_obj_t *ui_plrolComboThree;
lv_obj_t *ui_plrolComboFour;

void init_muxparentlock(lv_obj_t *ui_pnlContent) {
    lv_obj_set_flex_flow(ui_pnlContent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_plrolComboOne = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_plrolComboOne, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_plrolComboOne, 80);
    lv_obj_set_height(ui_plrolComboOne, 320);
    lv_obj_set_align(ui_plrolComboOne, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_plrolComboOne, lv_color_hex(0x857B0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboOne, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_plrolComboOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_plrolComboOne, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_plrolComboOne, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_plrolComboOne, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboOne, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_plrolComboOne, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboOne, 5, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(ui_plrolComboOne, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(ui_plrolComboOne, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(ui_plrolComboOne, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_plrolComboOne, 5, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(ui_plrolComboOne, lv_color_hex(0xF7E318), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboOne, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboOne, 5, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboOne, lv_color_hex(0x484207), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboOne, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

    ui_plrolComboTwo = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_plrolComboTwo, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_plrolComboTwo, 80);
    lv_obj_set_height(ui_plrolComboTwo, 320);
    lv_obj_set_align(ui_plrolComboTwo, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_plrolComboTwo, lv_color_hex(0x857B0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboTwo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_plrolComboTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_plrolComboTwo, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_plrolComboTwo, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_plrolComboTwo, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboTwo, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_plrolComboTwo, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboTwo, 5, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(ui_plrolComboTwo, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(ui_plrolComboTwo, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(ui_plrolComboTwo, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_plrolComboTwo, 5, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(ui_plrolComboTwo, lv_color_hex(0xF7E318), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboTwo, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboTwo, 5, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboTwo, lv_color_hex(0x484207), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboTwo, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

    ui_plrolComboThree = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_plrolComboThree, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_plrolComboThree, 80);
    lv_obj_set_height(ui_plrolComboThree, 320);
    lv_obj_set_align(ui_plrolComboThree, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_plrolComboThree, lv_color_hex(0x857B0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboThree, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_plrolComboThree, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_plrolComboThree, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_plrolComboThree, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_plrolComboThree, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboThree, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboThree, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboThree, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_plrolComboThree, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboThree, 5, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(ui_plrolComboThree, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(ui_plrolComboThree, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(ui_plrolComboThree, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_plrolComboThree, 5, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(ui_plrolComboThree, lv_color_hex(0xF7E318), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboThree, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboThree, 5, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboThree, lv_color_hex(0x484207), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboThree, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

    ui_plrolComboFour = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_plrolComboFour, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_plrolComboFour, 80);
    lv_obj_set_height(ui_plrolComboFour, 320);
    lv_obj_set_align(ui_plrolComboFour, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_plrolComboFour, lv_color_hex(0x857B0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboFour, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_plrolComboFour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_plrolComboFour, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_plrolComboFour, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_plrolComboFour, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboFour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboFour, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboFour, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_plrolComboFour, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboFour, 5, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(ui_plrolComboFour, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(ui_plrolComboFour, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(ui_plrolComboFour, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(ui_plrolComboFour, 5, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(ui_plrolComboFour, lv_color_hex(0xF7E318), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_plrolComboFour, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_plrolComboFour, 5, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_plrolComboFour, lv_color_hex(0x484207), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_plrolComboFour, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

}
