#include "ui.h"

lv_obj_t *ui_scrCredits;
lv_obj_t *ui_conCredits;
lv_obj_t *ui_conStart;
lv_obj_t *ui_lblStartTitle;
lv_obj_t *ui_lblStartMessage;
lv_obj_t *ui_conScroll;
lv_obj_t *ui_lblOfficialTitle;
lv_obj_t *ui_lblCommanderTitle;
lv_obj_t *ui_lblCommanderPeople;
lv_obj_t *ui_lblEnforcerTitle;
lv_obj_t *ui_lblEnforcerPeople;
lv_obj_t *ui_lblWizardTitle;
lv_obj_t *ui_lblWizardLeft;
lv_obj_t *ui_lblWizardRight;
lv_obj_t *ui_lblHeroTitle;
lv_obj_t *ui_lblHeroLeft;
lv_obj_t *ui_lblHeroRight;
lv_obj_t *ui_lblKnightTitle;
lv_obj_t *ui_lblKnightLeft;
lv_obj_t *ui_lblKnightRight;
lv_obj_t *ui_lblSpecialTitle;
lv_obj_t *ui_lblSpecialMid;
lv_obj_t *ui_conKofi;
lv_obj_t *ui_lblKofiTitle;
lv_obj_t *ui_lblKofiMessageOne;
lv_obj_t *ui_lblKofiMessageTwo;
lv_obj_t *ui_imgKofi;

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
    lv_anim_set_values(&PropertyAnimation_0, 1550, -2500);
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

void ui_init(void) {
    ui_scrCredits_screen_init();
    lv_obj_create(NULL);
    lv_disp_load_scr(ui_scrCredits);
}
