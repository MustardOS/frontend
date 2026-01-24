#include "ui_muxactivity.h"

lv_obj_t *ui_lblCounter_activity;

void init_muxactivity(lv_obj_t *ui_screen, struct theme_config *theme) {
    ui_lblCounter_activity = lv_label_create(ui_screen);
    lv_label_set_text(ui_lblCounter_activity, "");
    lv_obj_set_width(ui_lblCounter_activity, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblCounter_activity, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCounter_activity, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblCounter_activity, "");
    lv_obj_add_flag(ui_lblCounter_activity, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblCounter_activity, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_lblCounter_activity, lv_color_hex(theme->COUNTER.TEXT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCounter_activity, theme->COUNTER.TEXT_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCounter_activity, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblCounter_activity, lv_color_hex(theme->COUNTER.BACKGROUND), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblCounter_activity, theme->COUNTER.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCounter_activity, lv_color_hex(theme->COUNTER.BORDER_COLOUR), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCounter_activity, theme->COUNTER.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCounter_activity, theme->COUNTER.BORDER_WIDTH, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCounter_activity, theme->COUNTER.PADDING_AROUND, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCounter_activity, theme->COUNTER.PADDING_AROUND, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCounter_activity, theme->COUNTER.PADDING_AROUND, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCounter_activity, theme->COUNTER.PADDING_AROUND, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_lblCounter_activity, theme->COUNTER.RADIUS, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_y(ui_lblCounter_activity, theme->COUNTER.PADDING_TOP);
    switch (theme->COUNTER.ALIGNMENT) {
        case 1:
            lv_obj_set_align(ui_lblCounter_activity, LV_ALIGN_TOP_MID);
            break;
        case 2:
            lv_obj_set_align(ui_lblCounter_activity, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_x(ui_lblCounter_activity, -theme->COUNTER.PADDING_SIDE);
            break;
        default:
            lv_obj_set_align(ui_lblCounter_activity, LV_ALIGN_TOP_LEFT);
            lv_obj_set_x(ui_lblCounter_activity, theme->COUNTER.PADDING_SIDE);
            break;
    }
}
