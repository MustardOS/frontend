#include "ui.h"
#include "../../common/font/notosans.h"
#include "../../common/font/notosans_big.h"

void ui_scrCredits_screen_init(void) {
    ui_scrCredits = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrCredits, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scrCredits, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrCredits, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_scrCredits, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_conCredits = lv_obj_create(ui_scrCredits);
    lv_obj_remove_style_all(ui_conCredits);
    lv_obj_set_width(ui_conCredits, lv_pct(100));
    lv_obj_set_height(ui_conCredits, lv_pct(100));
    lv_obj_set_align(ui_conCredits, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_conCredits, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_conStart = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conStart);
    lv_obj_set_width(ui_conStart, lv_pct(100));
    lv_obj_set_height(ui_conStart, lv_pct(100));
    lv_obj_set_x(ui_conStart, lv_pct(0));
    lv_obj_set_y(ui_conStart, lv_pct(100));
    lv_obj_set_align(ui_conStart, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conStart, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conStart, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conStart, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conStart, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblStartTitle = lv_label_create(ui_conStart);
    lv_obj_set_width(ui_lblStartTitle, lv_pct(100));
    lv_obj_set_height(ui_lblStartTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblStartTitle, 0);
    lv_obj_set_y(ui_lblStartTitle, 20);
    lv_obj_set_align(ui_lblStartTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblStartTitle, "\n\n\n\n\nThank you for choosing muOS");
    lv_obj_set_scroll_dir(ui_lblStartTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblStartTitle, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblStartTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblStartTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblStartTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblStartTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblStartTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblStartTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblStartTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblStartTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblStartTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblStartTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblStartTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblStartTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblStartTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblStartMessage = lv_label_create(ui_conStart);
    lv_obj_set_width(ui_lblStartMessage, lv_pct(90));
    lv_obj_set_height(ui_lblStartMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblStartMessage, 0);
    lv_obj_set_y(ui_lblStartMessage, 30);
    lv_obj_set_align(ui_lblStartMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblStartMessage,
                      "\nThe following people play a huge part in the development and future for this custom firmware."
                      "\n\nFrom the bottom of my heart thank you so much."
                      "\nI appreciate you more than a simple thank you ever could.");
    lv_obj_set_scroll_dir(ui_lblStartMessage, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblStartMessage, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblStartMessage, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblStartMessage, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblStartMessage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblStartMessage, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblStartMessage, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblStartMessage, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblStartMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblStartMessage, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblStartMessage, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_conScroll = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conScroll);
    lv_obj_set_height(ui_conScroll, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_conScroll, lv_pct(100));
    lv_obj_set_y(ui_conScroll, 2450);
    lv_obj_set_x(ui_conScroll, lv_pct(0));
    lv_obj_set_align(ui_conScroll, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conScroll, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_conScroll, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(ui_conScroll, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ui_lblOfficialTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblOfficialTitle, 60);
    lv_obj_set_width(ui_lblOfficialTitle, lv_pct(100));
    lv_obj_set_x(ui_lblOfficialTitle, 0);
    lv_obj_set_y(ui_lblOfficialTitle, 20);
    lv_obj_set_align(ui_lblOfficialTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblOfficialTitle, "muOS Crew");
    lv_obj_set_scroll_dir(ui_lblOfficialTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblOfficialTitle, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblOfficialTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblOfficialTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblOfficialTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblOfficialTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblOfficialTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblOfficialTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblOfficialTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblOfficialTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblOfficialTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblOfficialTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblOfficialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblOfficialTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblOfficialTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblCommanderTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblCommanderTitle, 60);
    lv_obj_set_width(ui_lblCommanderTitle, lv_pct(100));
    lv_obj_set_align(ui_lblCommanderTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblCommanderTitle, "Commanders");
    lv_obj_set_scroll_dir(ui_lblCommanderTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblCommanderTitle, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCommanderTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCommanderTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblCommanderTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCommanderTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCommanderTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblCommanderTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCommanderTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblCommanderTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblCommanderTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblCommanderTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblCommanderPeople = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblCommanderPeople, lv_pct(100));
    lv_obj_set_height(ui_lblCommanderPeople, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCommanderPeople, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblCommanderPeople,
                      "xonglebongle\n"
                      "antiKk\n"
                      "Corey"
    );
    lv_obj_set_scroll_dir(ui_lblCommanderPeople, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblCommanderPeople, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCommanderPeople, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblCommanderPeople, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCommanderPeople, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblCommanderPeople, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCommanderPeople, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCommanderPeople, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblCommanderPeople, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblCommanderPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblCommanderPeople, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblCommanderPeople, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblEnforcerTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblEnforcerTitle, 60);
    lv_obj_set_width(ui_lblEnforcerTitle, lv_pct(100));
    lv_obj_set_align(ui_lblEnforcerTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblEnforcerTitle, "Enforcers");
    lv_obj_set_scroll_dir(ui_lblEnforcerTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblEnforcerTitle, lv_color_hex(0xFF4500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblEnforcerTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblEnforcerTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblEnforcerTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblEnforcerTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblEnforcerTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblEnforcerTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblEnforcerTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblEnforcerTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblEnforcerTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblEnforcerTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblEnforcerPeople = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblEnforcerPeople, lv_pct(100));
    lv_obj_set_height(ui_lblEnforcerPeople, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblEnforcerPeople, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblEnforcerPeople,
                      "acmeplus\n"
                      "bgelmini\n"
                      "Casey\n"
                      "Duncanyoyo1\n"
                      "Raven"
    );
    lv_obj_set_scroll_dir(ui_lblEnforcerPeople, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblEnforcerPeople, lv_color_hex(0xFF4500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblEnforcerPeople, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblEnforcerPeople, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblEnforcerPeople, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblEnforcerPeople, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblEnforcerPeople, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblEnforcerPeople, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblEnforcerPeople, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblEnforcerPeople, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblEnforcerPeople, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblEnforcerPeople, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblWizardTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblWizardTitle, 60);
    lv_obj_set_width(ui_lblWizardTitle, lv_pct(100));
    lv_obj_set_x(ui_lblWizardTitle, 0);
    lv_obj_set_y(ui_lblWizardTitle, 20);
    lv_obj_set_align(ui_lblWizardTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblWizardTitle, "Wizards");
    lv_obj_set_scroll_dir(ui_lblWizardTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblWizardTitle, lv_color_hex(0xED6900), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblWizardTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblWizardTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblWizardTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblWizardTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblWizardTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblWizardTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblWizardTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblWizardTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblWizardTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblWizardTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblWizardTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblWizardTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblWizardTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblWizardTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblWizardLeft = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblWizardLeft, lv_pct(50));
    lv_obj_set_height(ui_lblWizardLeft, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblWizardLeft, 0);
    lv_obj_set_y(ui_lblWizardLeft, 30);
    lv_obj_set_align(ui_lblWizardLeft, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblWizardLeft,
                      "kloptops\n"
                      "vagueparade\n"
                      "tokyovigilante\n"
                      "bcat\n"
                      "XQuader\n"
                      "siliconexarch\n"
                      "johnnyonflame\n"
                      "Cebion\n"
                      "Rosie\n"
                      "vq37vhrgang\n"
                      "Ajmandourah\n"
                      "birdie"
    );
    lv_obj_set_scroll_dir(ui_lblWizardLeft, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblWizardLeft, lv_color_hex(0xED6900), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblWizardLeft, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblWizardLeft, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblWizardLeft, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblWizardLeft, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblWizardLeft, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblWizardLeft, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblWizardLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblWizardLeft, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblWizardLeft, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblWizardRight = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblWizardRight, lv_pct(50));
    lv_obj_set_height(ui_lblWizardRight, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblWizardRight, 0);
    lv_obj_set_y(ui_lblWizardRight, 30);
    lv_obj_set_align(ui_lblWizardRight, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblWizardRight,
                      "e1000\n"
                      "Gamma\n"
                      "joyrider3774\n"
                      "Bitter_Bizarro\n"
                      "shengy\n"
                      "shauninman\n"
                      "snowram\n"
                      "trngaje\n"
                      "Skorpy\n"
                      "Stanley\n"
                      "irilvibi\n"
                      "MiDWaN\n"
    );
    lv_obj_set_scroll_dir(ui_lblWizardRight, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblWizardRight, lv_color_hex(0xED6900), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblWizardRight, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblWizardRight, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblWizardRight, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblWizardRight, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblWizardRight, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblWizardRight, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblWizardRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblWizardRight, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblWizardRight, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblHeroTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblHeroTitle, 60);
    lv_obj_set_width(ui_lblHeroTitle, lv_pct(100));
    lv_obj_set_x(ui_lblHeroTitle, 0);
    lv_obj_set_y(ui_lblHeroTitle, 20);
    lv_obj_set_align(ui_lblHeroTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblHeroTitle, "Heroes");
    lv_obj_set_scroll_dir(ui_lblHeroTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroTitle, lv_color_hex(0xFFDD22), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHeroTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblHeroTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblHeroTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblHeroTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblHeroTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblHeroTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblHeroLeft = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblHeroLeft, lv_pct(50));
    lv_obj_set_height(ui_lblHeroLeft, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblHeroLeft, 0);
    lv_obj_set_y(ui_lblHeroLeft, 30);
    lv_obj_set_align(ui_lblHeroLeft, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblHeroLeft,
                      "Eronauta (Benzo)\n"
                      "X-Tremis\n"
                      "Lmarcomiranda\n"
                      "LSDMTHC\n"
                      "lord | Hegen Deroes\n"
                      "inteLLiAim\n"
                      "Kenton\n"
                      "liloconf\n"
                      "Seth\n"
                      "Bazkart\n"
                      "msx\n"
                      "Joey's Retro Handhelds\n"
                      "sniper257\n"
                      "TeggyDave\n"
                      "Gabi_90\n"
                      "Robbie Trencheny\n"
                      "JanTrueno\n"
                      "Fabio C. Barrionuevo\n"
                      "Andrew\n"
                      "SuperZu\n"
                      "MrGinger\n"
                      "Kyle Faun\n"
                      "Iceman\n"
                      "Kaeltis\n"
                      "Andika\n"
                      "ReiTW\n"
                      "RaoulDook\n"
                      "GamerGuy1975\n"
                      "Benjamin Bercy\n"
                      "SNESFAN\n"
                      "MrGrin3H\n"
                      "Matrioshka Brain\n"
                      "Suribii\n"
                      "LuccaPucca\n"
                      "Gio\n"
                      "JimmyCrackedCorn_4711\n"
                      "Bearington\n"
                      "JayBee\n"
                      "AnIvarCalledTwO\n"
                      "Ben the Wizard\n"
                      "Lightning\n"
                      "Flame Sage\n"
                      "CrystalUnclear\n"
                      "Chonky Birb\n"
                      "alfre.gi\n"
                      "dririan\n"
                      "WattoOwnedVader\n"
                      "sol6_vi\n"
                      "lvalkyrie\n"
                      "littleMel\n"
                      "Timmernator\n"
                      "Roksta(Phil)\n"
                      "Cerebellium\n"
                      "mach"
    );
    lv_obj_set_scroll_dir(ui_lblHeroLeft, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroLeft, lv_color_hex(0xFFDD22), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroLeft, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblHeroLeft, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroLeft, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroLeft, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroLeft, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroLeft, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroLeft, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblHeroLeft, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblHeroRight = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblHeroRight, lv_pct(50));
    lv_obj_set_height(ui_lblHeroRight, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblHeroRight, 0);
    lv_obj_set_y(ui_lblHeroRight, 30);
    lv_obj_set_align(ui_lblHeroRight, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblHeroRight,
                      "N0FPV\n"
                      "rabite\n"
                      "zaka1w3\n"
                      "Nico_Linber\n"
                      "Linber\n"
                      "PR0J3KT2501\n"
                      "MeowMan\n"
                      "paletochen\n"
                      "r4ed\n"
                      "Avior\n"
                      "dandan the pandaman\n"
                      "illumini_85\n"
                      "HueyKablooey\n"
                      "Spartan_029\n"
                      "mrwhistles\n"
                      "the-mad-tinkerer\n"
                      "losermatic\n"
                      "intrudir\n"
                      "segawd\n"
                      "Rex\n"
                      "Etan\n"
                      "Sam Watson\n"
                      "TheSpivvBulb\n"
                      "Jelzer\n"
                      "jessiewonka\n"
                      "N3VURMYND\n"
                      "Frizzin\n"
                      "Dramafexx\n"
                      "Amos\n"
                      "hssnd\n"
                      "espy\n"
                      "tech/ian\n"
                      "Tacktula\n"
                      "SuperRAD\n"
                      "TeslaDelMar\n"
                      "DarkkMinion\n"
                      "John\n"
                      "Ylber\n"
                      "Diabl0z666\n"
                      "Kusal\n"
                      "Monty\n"
                      "Turner74\n"
                      "chiefwally\n"
                      "phantomcommander\n"
                      "Lukas Nowicki\n"
                      "BigFootHenders\n"
                      "TheManiacKY\n"
                      "Scy0n\n"
                      "mers_0000\n"
                      "LuckyPhil\n"
                      "Nack\n"
                      "DanOzGod\n"
                      "Danny"
    );
    lv_obj_set_scroll_dir(ui_lblHeroRight, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroRight, lv_color_hex(0xFFDD22), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroRight, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblHeroRight, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroRight, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroRight, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroRight, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroRight, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroRight, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblHeroRight, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblKnightTitle = lv_label_create(ui_conScroll);
    lv_obj_set_height(ui_lblKnightTitle, 60);
    lv_obj_set_width(ui_lblKnightTitle, lv_pct(100));
    lv_obj_set_x(ui_lblKnightTitle, 0);
    lv_obj_set_y(ui_lblKnightTitle, 20);
    lv_obj_set_align(ui_lblKnightTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblKnightTitle, "Knights");
    lv_obj_set_scroll_dir(ui_lblKnightTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightTitle, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKnightTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblKnightTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblKnightTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblKnightTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblKnightTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKnightTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblKnightLeft = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblKnightLeft, lv_pct(50));
    lv_obj_set_height(ui_lblKnightLeft, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblKnightLeft, 0);
    lv_obj_set_y(ui_lblKnightLeft, 30);
    lv_obj_set_align(ui_lblKnightLeft, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_lblKnightLeft,
                      "CR4YFish\n"
                      "flacko\n"
                      "SkyArcher\n"
                      "Gallo\n"
                      "PrestonC\n"
                      "Leonidas\n"
                      "Mrbadpour\n"
                      "Eric-RetroHandheldGuides\n"
                      "PhyrexTechs\n"
                      "Delored\n"
                      "Hex Maniack\n"
                      "kiko\n"
                      "arkholt\n"
                      "Starship9\n"
                      "fibroidjames\n"
                      "Allepac\n"
                      "Arobee\n"
                      "Pa'Wan\n"
                      "Cap\n"
                      "Luke\n"
                      "drisc\n"
                      "ClemPurp\n"
                      "RySpy\n"
                      "aj15\n"
                      "Ab\n"
                      "MrMinholi\n"
                      "RetroGameCorps\n"
                      "biffoutt\n"
                      "rbndr\n"
                      "ToastBomb\n"
                      "Furgus\n"
                      "NecroPoster (BillyNaing)\n"
                      "Nuke\n"
                      "deadfred69\n"
                      "MidnightRocker1270\n"
                      "Resonatur\n"
                      "Yami\n"
                      "rsyvn\n"
                      "EmptyWalletCrumbs\n"
                      "VJ425\n"
                      "Sole\n"
                      "thedreadpirates"
    );
    lv_obj_set_scroll_dir(ui_lblKnightLeft, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightLeft, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightLeft, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKnightLeft, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightLeft, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightLeft, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightLeft, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightLeft, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightLeft, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightLeft, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKnightLeft, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblKnightRight = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblKnightRight, lv_pct(50));
    lv_obj_set_height(ui_lblKnightRight, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblKnightRight, 0);
    lv_obj_set_y(ui_lblKnightRight, 30);
    lv_obj_set_align(ui_lblKnightRight, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(ui_lblKnightRight,
                      "GnobarEl\n"
                      "Smitty Werben Jaegerman Jensen\n"
                      "SickSadBilly\n"
                      "falafeless\n"
                      "leberwurstmitsauregurken\n"
                      "demiurge\n"
                      "Mahdi88\n"
                      "ripuk3.16\n"
                      "wizdude\n"
                      "Auro\n"
                      "abysmal\n"
                      "andornaut\n"
                      "vagueanx\n"
                      "lobster\n"
                      "FreezerBurn\n"
                      "MiniManul\n"
                      "Robot\n"
                      "surge (8263)\n"
                      "surge (84306)\n"
                      "BlueCaret\n"
                      "olli67\n"
                      "Luminite\n"
                      "deprowdp\n"
                      "Jaybird1998\n"
                      "JohnReynolds\n"
                      "reese\n"
                      "Totino\n"
                      "talhelfgott\n"
                      "tessesseff\n"
                      "cheez\n"
                      "Tani\n"
                      "trevor6710\n"
                      "phlurblepoot\n"
                      "Foggy\n"
                      "MrCee 1503\n"
                      "Splendid88\n"
                      "soma\n"
                      "TheTomothy\n"
                      "mamamiamoto\n"
                      "Nuke\n"
                      "Colorbl1nd"
    );
    lv_obj_set_scroll_dir(ui_lblKnightRight, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightRight, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightRight, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKnightRight, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightRight, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightRight, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightRight, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightRight, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightRight, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightRight, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKnightRight, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_conSpecial = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conSpecial);
    lv_obj_set_width(ui_conSpecial, lv_pct(100));
    lv_obj_set_height(ui_conSpecial, lv_pct(100));
    lv_obj_set_x(ui_conSpecial, lv_pct(0));
    lv_obj_set_y(ui_conSpecial, lv_pct(100));
    lv_obj_set_align(ui_conSpecial, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conSpecial, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conSpecial, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conSpecial, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conSpecial, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_conSpecial, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblSpecialTitle = lv_label_create(ui_conSpecial);
    lv_obj_set_height(ui_lblSpecialTitle, 60);
    lv_obj_set_width(ui_lblSpecialTitle, lv_pct(100));
    lv_obj_set_x(ui_lblSpecialTitle, 0);
    lv_obj_set_y(ui_lblSpecialTitle, 10);
    lv_obj_set_align(ui_lblSpecialTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblSpecialTitle, "Special Thanks");
    lv_obj_set_scroll_dir(ui_lblSpecialTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblSpecialTitle, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblSpecialTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblSpecialTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblSpecialTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblSpecialTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblSpecialTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblSpecialTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblSpecialTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblSpecialTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblSpecialTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblSpecialTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblSpecialTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblSpecialTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblSpecialTitle, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblSpecialTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblSpecialMid = lv_label_create(ui_conSpecial);
    lv_obj_set_width(ui_lblSpecialMid, lv_pct(90));
    lv_obj_set_height(ui_lblSpecialMid, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblSpecialMid, 0);
    lv_obj_set_y(ui_lblSpecialMid, 20);
    lv_obj_set_align(ui_lblSpecialMid, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblSpecialMid,
                      "A heartfelt thank you to the artificers, druids, porters, and translators for your invaluable "
                      "contributions to muOS. Your hard work and commitment have truly enriched this project, and I "
                      "am deeply grateful for each of you and the unique talents you bring!");
    lv_obj_set_scroll_dir(ui_lblSpecialMid, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblSpecialMid, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblSpecialMid, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblSpecialMid, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblSpecialMid, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblSpecialMid, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblSpecialMid, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblSpecialMid, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblSpecialMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblSpecialMid, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblSpecialMid, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblBongleTitle = lv_label_create(ui_conSpecial);
    lv_obj_set_height(ui_lblBongleTitle, 60);
    lv_obj_set_width(ui_lblBongleTitle, lv_pct(100));
    lv_obj_set_x(ui_lblBongleTitle, 0);
    lv_obj_set_y(ui_lblBongleTitle, 10);
    lv_obj_set_align(ui_lblBongleTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblBongleTitle, "My One True Supporter");
    lv_obj_set_scroll_dir(ui_lblBongleTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblBongleTitle, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBongleTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBongleTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblBongleTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblBongleTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblBongleTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblBongleTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblBongleTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblBongleTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblBongleTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblBongleTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblBongleTitle, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblBongleTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblBongleTitle, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblBongleTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblBongleMid = lv_label_create(ui_conSpecial);
    lv_obj_set_width(ui_lblBongleMid, lv_pct(90));
    lv_obj_set_height(ui_lblBongleMid, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblBongleMid, 0);
    lv_obj_set_y(ui_lblBongleMid, 20);
    lv_obj_set_align(ui_lblBongleMid, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblBongleMid,
                      "To my wonderful wife, Mrs. Bongle. Whose amazing support, guidance, and patience has been my "
                      "light throughout this entire adventure. You mean everything to me and I love you more than "
                      "words could ever express!");
    lv_obj_set_scroll_dir(ui_lblBongleMid, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblBongleMid, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBongleMid, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblBongleMid, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBongleMid, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblBongleMid, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblBongleMid, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblBongleMid, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblBongleMid, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblBongleMid, lv_color_hex(0x87C97C), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblBongleMid, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_conKofi = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conKofi);
    lv_obj_set_width(ui_conKofi, lv_pct(100));
    lv_obj_set_height(ui_conKofi, lv_pct(100));
    lv_obj_set_x(ui_conKofi, lv_pct(0));
    lv_obj_set_y(ui_conKofi, lv_pct(100));
    lv_obj_set_align(ui_conKofi, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conKofi, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conKofi, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conKofi, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conKofi, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_conKofi, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblKofiTitle = lv_label_create(ui_conKofi);
    lv_obj_set_width(ui_lblKofiTitle, lv_pct(100));
    lv_obj_set_height(ui_lblKofiTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblKofiTitle, 0);
    lv_obj_set_y(ui_lblKofiTitle, 20);
    lv_obj_set_align(ui_lblKofiTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblKofiTitle, "Thank you for choosing muOS");
    lv_obj_set_scroll_dir(ui_lblKofiTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiTitle, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKofiTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblKofiTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblKofiTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblKofiTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblKofiTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKofiTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblKofiMessageOne = lv_label_create(ui_conKofi);
    lv_obj_set_width(ui_lblKofiMessageOne, lv_pct(100));
    lv_obj_set_height(ui_lblKofiMessageOne, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblKofiMessageOne, 0);
    lv_obj_set_y(ui_lblKofiMessageOne, 30);
    lv_obj_set_align(ui_lblKofiMessageOne, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblKofiMessageOne,
                      "You can support muOS by donating or subscribing which helps the "
                      "development of this project. This project is done as a hobby!");
    lv_obj_set_scroll_dir(ui_lblKofiMessageOne, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiMessageOne, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiMessageOne, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKofiMessageOne, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiMessageOne, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiMessageOne, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiMessageOne, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiMessageOne, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiMessageOne, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiMessageOne, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKofiMessageOne, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblKofiMessageTwo = lv_label_create(ui_conKofi);
    lv_obj_set_width(ui_lblKofiMessageTwo, lv_pct(100));
    lv_obj_set_height(ui_lblKofiMessageTwo, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblKofiMessageTwo, 0);
    lv_obj_set_y(ui_lblKofiMessageTwo, 30);
    lv_obj_set_align(ui_lblKofiMessageTwo, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblKofiMessageTwo, "Scanning the below QR Code will take you to the Ko-fi page!\n");
    lv_obj_set_scroll_dir(ui_lblKofiMessageTwo, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKofiMessageTwo, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKofiMessageTwo, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKofiMessageTwo, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKofiMessageTwo, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKofiMessageTwo, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKofiMessageTwo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKofiMessageTwo, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKofiMessageTwo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKofiMessageTwo, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKofiMessageTwo, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_imgKofi = lv_img_create(ui_conKofi);
    lv_img_set_src(ui_imgKofi, &ui_image_Kofi);
    lv_obj_set_width(ui_imgKofi, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_imgKofi, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_imgKofi, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgKofi, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgKofi, LV_OBJ_FLAG_SCROLLABLE);
}
