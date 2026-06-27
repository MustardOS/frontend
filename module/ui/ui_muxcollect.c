#include "ui_muxcollect.h"
#include "../../common/device.h"

lv_obj_t *ui_lbl_counter_collect;

lv_obj_t *ui_pnl_entry_collect;
lv_obj_t *ui_txt_entry_collect;

void init_muxcollect(lv_obj_t *ui_screen, const struct theme_config *theme) {
    ui_lbl_counter_collect = lv_label_create(ui_screen);
    lv_label_set_text(ui_lbl_counter_collect, "");
    lv_obj_set_width(ui_lbl_counter_collect, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lbl_counter_collect, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_counter_collect, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbl_counter_collect, "");
    lv_obj_add_flag(ui_lbl_counter_collect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lbl_counter_collect, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_lbl_counter_collect, lv_color_hex(theme->counter.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_counter_collect, theme->counter.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_counter_collect, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_lbl_counter_collect, lv_color_hex(theme->counter.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lbl_counter_collect, theme->counter.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(
        ui_lbl_counter_collect, lv_color_hex(theme->counter.border_colour), MU_OBJ_MAIN_DEFAULT
    );
    lv_obj_set_style_border_opa(ui_lbl_counter_collect, theme->counter.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_lbl_counter_collect, theme->counter.border_width, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_lbl_counter_collect, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_lbl_counter_collect, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_lbl_counter_collect, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lbl_counter_collect, theme->counter.padding_around, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_lbl_counter_collect, theme->counter.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_y(ui_lbl_counter_collect, theme->counter.padding_top);

    switch (theme->counter.alignment) {
        case 1:
            lv_obj_set_align(ui_lbl_counter_collect, LV_ALIGN_TOP_MID);
            break;
        case 2:
            lv_obj_set_align(ui_lbl_counter_collect, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_x(ui_lbl_counter_collect, -theme->counter.padding_side);
            break;
        default:
            lv_obj_set_align(ui_lbl_counter_collect, LV_ALIGN_TOP_LEFT);
            lv_obj_set_x(ui_lbl_counter_collect, theme->counter.padding_side);
            break;
    }

    ui_pnl_entry_collect = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_collect, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_collect, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_collect, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_collect, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_collect, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_collect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_collect, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_collect, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_collect, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_collect, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_collect, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_collect, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_collect = lv_textarea_create(ui_pnl_entry_collect);
    lv_obj_set_width(ui_txt_entry_collect, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_collect, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_collect, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_collect, 1024);
    lv_textarea_set_one_line(ui_txt_entry_collect, 1);
    lv_obj_set_style_radius(ui_txt_entry_collect, theme->osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_collect, lv_color_hex(theme->osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_collect, theme->osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_collect, 2, MU_OBJ_MAIN_DEFAULT);
}
