#include "ui_muxcredits.h"
#include "../../common/device.h"
#include "../../font/notosans.h"
#include "../../font/notosans_big.h"

lv_obj_t *ui_scrCredits;

lv_obj_t *ui_conCredits;
lv_obj_t *ui_conStart;
lv_obj_t *ui_conScroll;
lv_obj_t *ui_conSpecial;
lv_obj_t *ui_conKofi;
lv_obj_t *ui_conMusic;

lv_obj_t *ui_imgKofi;

lv_obj_t *ui_lblStartTitle;
lv_obj_t *ui_lblStartMessage;

lv_obj_t *ui_lblOfficialTitle;

lv_obj_t *ui_lblCommanderTitle;
lv_obj_t *ui_lblCommanderCrew;

lv_obj_t *ui_lblEnforcerTitle;
lv_obj_t *ui_lblEnforcerCrew;

lv_obj_t *ui_lblWizardTitle;
lv_obj_t *ui_lblWizardCrew;

lv_obj_t *ui_lblHeroTitle;
lv_obj_t *ui_lblHeroCrew;

lv_obj_t *ui_lblKnightTitle;
lv_obj_t *ui_lblKnightCrew;

lv_obj_t *ui_lblSpecialTitle;
lv_obj_t *ui_lblSpecialMid;

lv_obj_t *ui_lblBongleTitle;
lv_obj_t *ui_lblBongleMid;

lv_obj_t *ui_lblKofiTitle;
lv_obj_t *ui_lblKofiMessageOne;
lv_obj_t *ui_lblKofiMessageTwo;

lv_obj_t *ui_lblMusicTitle;
lv_obj_t *ui_lblMusicMessage;

void ui_anim_callback_free_user_data(lv_anim_t *a) {
    lv_mem_free(a->user_data);
    a->user_data = NULL;
}

void ui_anim_callback_set_y(lv_anim_t *a, int32_t v) {
    ui_anim_user_data_t *usr = (ui_anim_user_data_t *) a->user_data;
    lv_obj_set_y(usr->target, v);
}

void ui_anim_callback_set_opacity(lv_anim_t *a, int32_t v) {
    ui_anim_user_data_t *usr = (ui_anim_user_data_t *) a->user_data;
    lv_obj_set_style_opa(usr->target, v, 0);
}

int32_t ui_anim_callback_get_y(lv_anim_t *a) {
    ui_anim_user_data_t *usr = (ui_anim_user_data_t *) a->user_data;
    return lv_obj_get_y_aligned(usr->target);
}

int32_t ui_anim_callback_get_opacity(lv_anim_t *a) {
    ui_anim_user_data_t *usr = (ui_anim_user_data_t *) a->user_data;
    return lv_obj_get_style_opa(usr->target, 0);
}

void animScroll_Animation(lv_obj_t *TargetObject, int delay) {
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    PropertyAnimation_0_user_data->val = -1;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 60000);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, ui_anim_callback_set_y);
    lv_anim_set_values(&PropertyAnimation_0, 3648, -3648);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_linear);
    lv_anim_set_delay(&PropertyAnimation_0, delay + 0);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_0, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
    lv_anim_set_early_apply(&PropertyAnimation_0, false);
    lv_anim_start(&PropertyAnimation_0);
}

void animFade_Animation(lv_obj_t *TargetObject, int delay) {
    ui_anim_user_data_t *PropertyAnimation_0_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_0_user_data->target = TargetObject;
    PropertyAnimation_0_user_data->val = -1;
    lv_anim_t PropertyAnimation_0;
    lv_anim_init(&PropertyAnimation_0);
    lv_anim_set_time(&PropertyAnimation_0, 0);
    lv_anim_set_user_data(&PropertyAnimation_0, PropertyAnimation_0_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_0, ui_anim_callback_set_opacity);
    lv_anim_set_values(&PropertyAnimation_0, 0, 0);
    lv_anim_set_path_cb(&PropertyAnimation_0, lv_anim_path_linear);
    lv_anim_set_delay(&PropertyAnimation_0, delay + 0);
    lv_anim_set_deleted_cb(&PropertyAnimation_0, ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_0, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_0, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_0, 0);
    lv_anim_set_early_apply(&PropertyAnimation_0, true);
    lv_anim_start(&PropertyAnimation_0);
    ui_anim_user_data_t *PropertyAnimation_1_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_1_user_data->target = TargetObject;
    PropertyAnimation_1_user_data->val = -1;
    lv_anim_t PropertyAnimation_1;
    lv_anim_init(&PropertyAnimation_1);
    lv_anim_set_time(&PropertyAnimation_1, 3000);
    lv_anim_set_user_data(&PropertyAnimation_1, PropertyAnimation_1_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_1, ui_anim_callback_set_y);
    lv_anim_set_values(&PropertyAnimation_1, 0, -100);
    lv_anim_set_path_cb(&PropertyAnimation_1, lv_anim_path_ease_out);
    lv_anim_set_delay(&PropertyAnimation_1, delay + 0);
    lv_anim_set_deleted_cb(&PropertyAnimation_1, ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_1, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_1, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_1, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_1, 0);
    lv_anim_set_early_apply(&PropertyAnimation_1, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_1, &ui_anim_callback_get_y);
    lv_anim_start(&PropertyAnimation_1);
    ui_anim_user_data_t *PropertyAnimation_2_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_2_user_data->target = TargetObject;
    PropertyAnimation_2_user_data->val = -1;
    lv_anim_t PropertyAnimation_2;
    lv_anim_init(&PropertyAnimation_2);
    lv_anim_set_time(&PropertyAnimation_2, 1000);
    lv_anim_set_user_data(&PropertyAnimation_2, PropertyAnimation_2_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_2, ui_anim_callback_set_opacity);
    lv_anim_set_values(&PropertyAnimation_2, 0, 255);
    lv_anim_set_path_cb(&PropertyAnimation_2, lv_anim_path_ease_out);
    lv_anim_set_delay(&PropertyAnimation_2, delay + 3000);
    lv_anim_set_deleted_cb(&PropertyAnimation_2, ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_2, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_2, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_2, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_2, 0);
    lv_anim_set_early_apply(&PropertyAnimation_2, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_2, &ui_anim_callback_get_opacity);
    lv_anim_start(&PropertyAnimation_2);
    ui_anim_user_data_t *PropertyAnimation_3_user_data = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    PropertyAnimation_3_user_data->target = TargetObject;
    PropertyAnimation_3_user_data->val = -1;
    lv_anim_t PropertyAnimation_3;
    lv_anim_init(&PropertyAnimation_3);
    lv_anim_set_time(&PropertyAnimation_3, 3000);
    lv_anim_set_user_data(&PropertyAnimation_3, PropertyAnimation_3_user_data);
    lv_anim_set_custom_exec_cb(&PropertyAnimation_3, ui_anim_callback_set_opacity);
    lv_anim_set_values(&PropertyAnimation_3, 255, 2);
    lv_anim_set_path_cb(&PropertyAnimation_3, lv_anim_path_ease_in);
    lv_anim_set_delay(&PropertyAnimation_3, delay + 9000);
    lv_anim_set_deleted_cb(&PropertyAnimation_3, ui_anim_callback_free_user_data);
    lv_anim_set_playback_time(&PropertyAnimation_3, 0);
    lv_anim_set_playback_delay(&PropertyAnimation_3, 0);
    lv_anim_set_repeat_count(&PropertyAnimation_3, 0);
    lv_anim_set_repeat_delay(&PropertyAnimation_3, 0);
    lv_anim_set_early_apply(&PropertyAnimation_3, false);
    lv_anim_set_get_value_cb(&PropertyAnimation_3, &ui_anim_callback_get_opacity);
    lv_anim_start(&PropertyAnimation_3);
}

void init_mux(void) {
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
    lv_obj_set_style_bg_color(ui_conStart, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_conStart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
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
    lv_obj_set_y(ui_conScroll, device.SCREEN.HEIGHT + 3148);
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

    ui_lblCommanderCrew = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblCommanderCrew, lv_pct(100));
    lv_obj_set_height(ui_lblCommanderCrew, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCommanderCrew, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblCommanderCrew,
                      "xonglebongle\n"
                      "antikk\n"
                      "corey"
    );
    lv_obj_set_scroll_dir(ui_lblCommanderCrew, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblCommanderCrew, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCommanderCrew, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblCommanderCrew, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCommanderCrew, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblCommanderCrew, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCommanderCrew, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCommanderCrew, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblCommanderCrew, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblCommanderCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblCommanderCrew, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblCommanderCrew, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

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

    ui_lblEnforcerCrew = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblEnforcerCrew, lv_pct(100));
    lv_obj_set_height(ui_lblEnforcerCrew, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblEnforcerCrew, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblEnforcerCrew,
                      "acmeplus\n"
                      "bgelmini\n"
                      "ilfordhp5\n"
                      "duncanyoyo1\n"
                      "ravenstoroses\n"
                      "illumini_85"
    );
    lv_obj_set_scroll_dir(ui_lblEnforcerCrew, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblEnforcerCrew, lv_color_hex(0xFF4500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblEnforcerCrew, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblEnforcerCrew, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblEnforcerCrew, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblEnforcerCrew, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblEnforcerCrew, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblEnforcerCrew, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblEnforcerCrew, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblEnforcerCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblEnforcerCrew, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblEnforcerCrew, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

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

    ui_lblWizardCrew = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblWizardCrew, lv_pct(100));
    lv_obj_set_height(ui_lblWizardCrew, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblWizardCrew, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblWizardCrew,
                      "skorpy\n"
                      "trngaje\n"
                      "vq37vhrgang\n"
                      "snowram\n"
                      "johnnyonflame\n"
                      "shauninman\n"
                      "siliconexarch\n"
                      "shengy.\n"
                      "ajmandourah\n"
                      "stanley_00\n"
                      "brooodie.\n"
                      "rosemelody254\n"
                      "midwan\n"
                      "bitter_bizarro\n"
                      "bcat24\n"
                      "vagueparade\n"
                      "irilivibi\n"
                      ".cebion\n"
                      "joyrider3774\n"
                      "jupyter.\n"
                      ".tokyovigilante\n"
                      "thegammasqueeze\n"
                      "kloptops\n"
                      "ee1000\n"
                      "xquader\n"
                      "retrogfx_"
    );
    lv_obj_set_scroll_dir(ui_lblWizardCrew, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblWizardCrew, lv_color_hex(0xED6900), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblWizardCrew, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblWizardCrew, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblWizardCrew, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblWizardCrew, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblWizardCrew, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblWizardCrew, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblWizardCrew, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblWizardCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblWizardCrew, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblWizardCrew, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

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

    ui_lblHeroCrew = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblHeroCrew, lv_pct(100));
    lv_obj_set_height(ui_lblHeroCrew, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblHeroCrew, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblHeroCrew,
                      "rokstaphil\n"
                      "hybrid_sith\n"
                      "phantomcommander0393\n"
                      "xraygoggles\n"
                      "foamygames\n"
                      "nrmazloo\n"
                      "mach5682\n"
                      "djwyman\n"
                      "nahck\n"
                      "luckyphil\n"
                      "rita.rosea\n"
                      "scy0n\n"
                      "themaniacky\n"
                      "bigfoothenders\n"
                      "chiefwally_73445\n"
                      "turner74.\n"
                      "roundpi\n"
                      "meanager\n"
                      "ayan4m1\n"
                      "techagent\n"
                      "amos_06286\n"
                      "supremedialects\n"
                      "frizzin78\n"
                      "qpla\n"
                      "n3vurmynd\n"
                      "littlemel\n"
                      "jelzer.\n"
                      "sol6_vi\n"
                      "spivvmeister\n"
                      "just_bri\n"
                      ".dririan\n"
                      "piatypos\n"
                      "etanrepus\n"
                      "retrocn_global_shop\n"
                      "benthewizard.\n"
                      "ivar2028\n"
                      "losermatic\n"
                      "jaybee82\n"
                      "themadtinkerer.\n"
                      "mrwhistles\n"
                      "x_tremis\n"
                      "romus85\n"
                      "grimwtf\n"
                      "hueykablooey\n"
                      "opinion_panda\n"
                      ".aviorxk\n"
                      "jimmycrackedcorn_4711\n"
                      "mrrobotsk\n"
                      "luccapucca\n"
                      "suribii\n"
                      "matrioshkabrain\n"
                      "snesfan1\n"
                      "benjaminbercy\n"
                      "paletochen\n"
                      "gamerguy1975\n"
                      "raouldook.\n"
                      "reitw\n"
                      "kaeltis\n"
                      "meowman_\n"
                      "bigbossman0816\n"
                      "pr0j3kt2501\n"
                      ".mrginger\n"
                      "nico_linber\n"
                      "nico_linber_36894\n"
                      "superzu\n"
                      "zaka1w3\n"
                      "brohsnbluffs\n"
                      "luzfcb\n"
                      "rabite890\n"
                      "robbiet480\n"
                      "zazouboy\n"
                      "gabi_90\n"
                      "teggydave\n"
                      "btreecat\n"
                      "sniper_257\n"
                      "joeysretrohandhelds\n"
                      "msx6011\n"
                      "bazkart\n"
                      "sethg911\n"
                      "liloconf\n"
                      "kentonftw\n"
                      "intelliaim\n"
                      "youraveragelord\n"
                      "_lsdmthc\n"
                      "jottenmiller\n"
                      "lmarcomiranda"
    );
    lv_obj_set_scroll_dir(ui_lblHeroCrew, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblHeroCrew, lv_color_hex(0xFFDD22), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblHeroCrew, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblHeroCrew, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblHeroCrew, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblHeroCrew, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblHeroCrew, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblHeroCrew, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblHeroCrew, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblHeroCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblHeroCrew, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblHeroCrew, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

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

    ui_lblKnightCrew = lv_label_create(ui_conScroll);
    lv_obj_set_width(ui_lblKnightCrew, lv_pct(100));
    lv_obj_set_height(ui_lblKnightCrew, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblKnightCrew, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblKnightCrew,
                      "andreabrantes_55196\n"
                      "miakhalifaisoverrated\n"
                      "__soma\n"
                      "splendid88_98891\n"
                      "mrcee1503\n"
                      "phlurblepoot\n"
                      "tessesseff\n"
                      "smittywerbenjaegermanjensen9250\n"
                      "totino.\n"
                      "luminite3658\n"
                      "surge_84306\n"
                      "rubenius\n"
                      "admiralthrawn_1\n"
                      "cyan_cupcake\n"
                      "_wizdude\n"
                      "ripuk3.16\n"
                      ".mahdi88\n"
                      "demiurge9708\n"
                      "falafeless\n"
                      "gnobarel\n"
                      "dreadpirates\n"
                      "._sole_.\n"
                      "ghostinput\n"
                      "vj425.90\n"
                      "scottieboysname\n"
                      "milotorou\n"
                      "resonatur\n"
                      "midnightrocker1270\n"
                      "deadfred69\n"
                      "nuke_67641\n"
                      "gasqbduv\n"
                      "billynaing\n"
                      "furgus.\n"
                      "triplejj52\n"
                      ".retrogamingmonkey\n"
                      "ban675\n"
                      "sanelessone\n"
                      "rbndr_\n"
                      "biffout\n"
                      "retrogamecorps\n"
                      "aj15\n"
                      ".ryspy\n"
                      "clempurp9868\n"
                      "drisc\n"
                      "angry_cinnabon\n"
                      "xluuke\n"
                      "heyiscap.\n"
                      "pakwan8234\n"
                      "allepac\n"
                      "fibroidjames\n"
                      ".starship9\n"
                      "julas8799\n"
                      "arkholt\n"
                      "kiko_lake\n"
                      "hex_maniack\n"
                      "wmaddler\n"
                      "delored\n"
                      "phyrex\n"
                      "status.quo.exile\n"
                      "_maxwellsdemon\n"
                      "ben819.\n"
                      "galloc\n"
                      "skyarcher\n"
                      "jdanteq_18123\n"
                      "notflacko\n"
                      "allanc5963\n"
                      "poppajonzz"
    );
    lv_obj_set_scroll_dir(ui_lblKnightCrew, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblKnightCrew, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblKnightCrew, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblKnightCrew, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblKnightCrew, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblKnightCrew, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblKnightCrew, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblKnightCrew, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblKnightCrew, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblKnightCrew, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblKnightCrew, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblKnightCrew, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

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
                      "A heartfelt thank you to the artificers, druids, porters, theme creators, and translators "
                      "for your invaluable contributions to muOS. Your hard work and commitment have truly enriched "
                      "this project, and I am deeply grateful for each of you and the unique talents you bring!");
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

    ui_conMusic = lv_obj_create(ui_conCredits);
    lv_obj_remove_style_all(ui_conMusic);
    lv_obj_set_width(ui_conMusic, lv_pct(100));
    lv_obj_set_height(ui_conMusic, lv_pct(100));
    lv_obj_set_x(ui_conMusic, lv_pct(0));
    lv_obj_set_y(ui_conMusic, lv_pct(100));
    lv_obj_set_align(ui_conMusic, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_conMusic, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_conMusic, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_conMusic, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_clear_flag(ui_conMusic, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_conMusic, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblMusicTitle = lv_label_create(ui_conMusic);
    lv_obj_set_width(ui_lblMusicTitle, lv_pct(100));
    lv_obj_set_height(ui_lblMusicTitle, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblMusicTitle, 0);
    lv_obj_set_y(ui_lblMusicTitle, 20);
    lv_obj_set_align(ui_lblMusicTitle, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_lblMusicTitle, "\n\n\n\n\nSupporter Music");
    lv_obj_set_scroll_dir(ui_lblMusicTitle, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblMusicTitle, lv_color_hex(0xF7E318), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMusicTitle, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMusicTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblMusicTitle, &ui_font_NotoSansBig, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblMusicTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_lblMusicTitle, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_lblMusicTitle, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_lblMusicTitle, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblMusicTitle, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblMusicTitle, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblMusicTitle, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblMusicTitle, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblMusicTitle, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblMusicTitle, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    ui_lblMusicMessage = lv_label_create(ui_conMusic);
    lv_obj_set_width(ui_lblMusicMessage, lv_pct(90));
    lv_obj_set_height(ui_lblMusicMessage, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblMusicMessage, 0);
    lv_obj_set_y(ui_lblMusicMessage, 30);
    lv_obj_set_align(ui_lblMusicMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblMusicMessage,
                      "\nTrack: Final Frontier"
                      "\nComposer: Nimn One");
    lv_obj_set_scroll_dir(ui_lblMusicMessage, LV_DIR_HOR);
    lv_obj_set_style_text_color(ui_lblMusicMessage, lv_color_hex(0xDDA200), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMusicMessage, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui_lblMusicMessage, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMusicMessage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblMusicMessage, lv_color_hex(0xA5B2B5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblMusicMessage, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_lblMusicMessage, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_lblMusicMessage, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lblMusicMessage, lv_color_hex(0xF8E008), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_text_opa(ui_lblMusicMessage, 255, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_disp_load_scr(ui_scrCredits);
}
