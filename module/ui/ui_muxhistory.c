#include "ui_muxhistory.h"

lv_obj_t *ui_lbl_counter_history;

void init_muxhistory(lv_obj_t *ui_screen, const struct theme_config *theme) {
    ui_lbl_counter_history = lv_label_create(ui_screen);
    lv_label_set_text(ui_lbl_counter_history, "");
    lv_obj_set_width(ui_lbl_counter_history, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_counter_history, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_counter_history, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbl_counter_history, "");
    lv_obj_add_flag(ui_lbl_counter_history, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_counter_history, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_lbl_counter_history, lv_color_hex(theme->counter.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_counter_history, theme->counter.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_counter_history, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lbl_counter_history, lv_color_hex(theme->counter.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_counter_history, theme->counter.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(
        ui_lbl_counter_history, lv_color_hex(theme->counter.border_colour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_border_opa(ui_lbl_counter_history, theme->counter.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lbl_counter_history, theme->counter.border_width, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_counter_history, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_counter_history, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_counter_history, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_counter_history, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_lbl_counter_history, theme->counter.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_y(ui_lbl_counter_history, theme->counter.padding_top);
    switch (theme->counter.alignment) {
        case 1:
            lv_obj_set_align(ui_lbl_counter_history, LV_ALIGN_TOP_MID);
            break;
        case 2:
            lv_obj_set_align(ui_lbl_counter_history, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_x(ui_lbl_counter_history, -theme->counter.padding_side);
            break;
        default:
            lv_obj_set_align(ui_lbl_counter_history, LV_ALIGN_TOP_LEFT);
            lv_obj_set_x(ui_lbl_counter_history, theme->counter.padding_side);
            break;
    }
}
