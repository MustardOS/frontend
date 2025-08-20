#include "ui_muxpass.h"
#include "../../font/notosans_big.h"

lv_obj_t *ui_rolComboOne;
lv_obj_t *ui_rolComboTwo;
lv_obj_t *ui_rolComboThree;
lv_obj_t *ui_rolComboFour;
lv_obj_t *ui_rolComboFive;
lv_obj_t *ui_rolComboSix;

void init_muxpass(lv_obj_t *ui_pnlContent) {
    lv_obj_set_flex_flow(ui_pnlContent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_rolComboOne = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboOne, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboOne, 80);
    lv_obj_set_height(ui_rolComboOne, 320);
    lv_obj_set_align(ui_rolComboOne, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboOne, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboOne, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboOne, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboOne, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboOne, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboOne, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboOne, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboOne, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboOne, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboOne, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboOne, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboOne, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboOne, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboOne, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboOne, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboOne, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboOne, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboOne, 255, MU_OBJ_SELECT_DEFAULT);

    ui_rolComboTwo = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboTwo, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboTwo, 80);
    lv_obj_set_height(ui_rolComboTwo, 320);
    lv_obj_set_align(ui_rolComboTwo, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboTwo, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboTwo, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboTwo, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboTwo, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboTwo, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboTwo, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboTwo, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboTwo, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboTwo, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboTwo, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboTwo, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboTwo, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboTwo, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboTwo, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboTwo, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboTwo, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboTwo, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboTwo, 255, MU_OBJ_SELECT_DEFAULT);

    ui_rolComboThree = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboThree, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboThree, 80);
    lv_obj_set_height(ui_rolComboThree, 320);
    lv_obj_set_align(ui_rolComboThree, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboThree, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboThree, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboThree, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboThree, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboThree, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboThree, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboThree, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboThree, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboThree, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboThree, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboThree, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboThree, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboThree, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboThree, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboThree, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboThree, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboThree, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboThree, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboThree, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboThree, 255, MU_OBJ_SELECT_DEFAULT);

    ui_rolComboFour = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboFour, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboFour, 80);
    lv_obj_set_height(ui_rolComboFour, 320);
    lv_obj_set_align(ui_rolComboFour, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboFour, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboFour, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboFour, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboFour, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboFour, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboFour, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFour, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboFour, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboFour, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboFour, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFour, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboFour, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboFour, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboFour, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboFour, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboFour, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboFour, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFour, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboFour, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboFour, 255, MU_OBJ_SELECT_DEFAULT);

    ui_rolComboFive = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboFive, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboFive, 80);
    lv_obj_set_height(ui_rolComboFive, 320);
    lv_obj_set_align(ui_rolComboFive, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboFive, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboFive, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboFive, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboFive, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboFive, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboFive, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFive, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboFive, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboFive, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboFive, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFive, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboFive, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboFive, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboFive, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboFive, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboFive, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboFive, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboFive, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboFive, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboFive, 255, MU_OBJ_SELECT_DEFAULT);

    ui_rolComboSix = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(ui_rolComboSix, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(ui_rolComboSix, 80);
    lv_obj_set_height(ui_rolComboSix, 320);
    lv_obj_set_align(ui_rolComboSix, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_rolComboSix, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboSix, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_rolComboSix, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(ui_rolComboSix, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_rolComboSix, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(ui_rolComboSix, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboSix, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboSix, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboSix, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_rolComboSix, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboSix, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(ui_rolComboSix, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(ui_rolComboSix, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(ui_rolComboSix, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(ui_rolComboSix, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(ui_rolComboSix, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(ui_rolComboSix, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(ui_rolComboSix, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(ui_rolComboSix, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(ui_rolComboSix, 255, MU_OBJ_SELECT_DEFAULT);
}
