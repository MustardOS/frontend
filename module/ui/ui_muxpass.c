#include "ui_muxpass.h"
#include "../../font/notosans_big.h"

lv_obj_t *ui_rolComboOne;
lv_obj_t *ui_rolComboTwo;
lv_obj_t *ui_rolComboThree;
lv_obj_t *ui_rolComboFour;
lv_obj_t *ui_rolComboFive;
lv_obj_t *ui_rolComboSix;

static void create_roller(lv_obj_t ** roller, lv_obj_t * ui_pnlContent) {
    *roller = lv_roller_create(ui_pnlContent);
    lv_roller_set_options(*roller, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\nA\nB\nC\nD\nE\nF", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(*roller, 80);
    lv_obj_set_height(*roller, 320);
    lv_obj_set_align(*roller, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(*roller, lv_color_hex(0x857B0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(*roller, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(*roller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(*roller, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(*roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*roller, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*roller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(*roller, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(*roller, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(*roller, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*roller, 5, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(*roller, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(*roller, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(*roller, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_pad(*roller, 5, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_set_style_text_color(*roller, lv_color_hex(0xF7E318), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(*roller, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*roller, 5, LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(*roller, lv_color_hex(0x484207), LV_PART_SELECTED | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(*roller, 255, LV_PART_SELECTED | LV_STATE_DEFAULT);

}

void init_muxpass(lv_obj_t *ui_pnlContent) {
    lv_obj_set_flex_flow(ui_pnlContent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_roller(&ui_rolComboOne, ui_pnlContent);
    create_roller(&ui_rolComboTwo, ui_pnlContent);
    create_roller(&ui_rolComboThree, ui_pnlContent);
    create_roller(&ui_rolComboFour, ui_pnlContent);
    create_roller(&ui_rolComboFive, ui_pnlContent);
    create_roller(&ui_rolComboSix, ui_pnlContent);
}
