#include "ui_muxpass.h"
#include "../../font/notosans_big.h"

lv_obj_t *ui_rolComboOne;
lv_obj_t *ui_rolComboTwo;
lv_obj_t *ui_rolComboThree;
lv_obj_t *ui_rolComboFour;
lv_obj_t *ui_rolComboFive;
lv_obj_t *ui_rolComboSix;

static const char *ROLLER_OPTS = "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF";

static lv_obj_t *make_pass_roller(lv_obj_t *parent) {
    lv_obj_t * r = lv_roller_create(parent);

    lv_roller_set_options(r, ROLLER_OPTS, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_size(r, 80, 320);
    lv_obj_set_align(r, LV_ALIGN_CENTER);

    lv_obj_set_style_text_color(r, lv_color_hex(0x857B0F), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(r, 255, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_letter_space(r, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_line_space(r, 24, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_font(r, &ui_font_NotoSansBig, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(r, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(r, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(r, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_radius(r, 5, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_color(r, lv_color_hex(0xF7E318), MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_opa(r, 255, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_width(r, 2, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_outline_pad(r, 5, MU_OBJ_MAIN_FOCUS);

    lv_obj_set_style_text_color(r, lv_color_hex(0xF7E318), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_text_opa(r, 255, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_radius(r, 5, MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x484207), MU_OBJ_SELECT_DEFAULT);
    lv_obj_set_style_bg_opa(r, 255, MU_OBJ_SELECT_DEFAULT);

    return r;
}

void init_muxpass(lv_obj_t *ui_pnlContent) {
    lv_obj_set_flex_flow(ui_pnlContent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *rollers[6];
    for (int i = 0; i < 6; ++i) {
        rollers[i] = make_pass_roller(ui_pnlContent);
    }

    ui_rolComboOne = rollers[0];
    ui_rolComboTwo = rollers[1];
    ui_rolComboThree = rollers[2];
    ui_rolComboFour = rollers[3];
    ui_rolComboFive = rollers[4];
    ui_rolComboSix = rollers[5];
}
