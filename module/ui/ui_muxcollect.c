#include "ui_muxcollect.h"
#include "../../common/device.h"

lv_obj_t *ui_lblCounter;

lv_obj_t *ui_pnlEntry;
lv_obj_t *ui_txtEntry;

void init_mux(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_lblCounter = lv_label_create(ui_screen);
    lv_obj_set_width(ui_lblCounter, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_lblCounter, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCounter, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblCounter, "");
    lv_obj_add_flag(ui_lblCounter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_lblCounter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(ui_lblCounter, lv_color_hex(theme->COUNTER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCounter, theme->COUNTER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCounter, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_lblCounter, lv_color_hex(theme->COUNTER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_lblCounter, theme->COUNTER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_lblCounter, lv_color_hex(theme->COUNTER.BORDER_COLOUR),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_lblCounter, theme->COUNTER.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_lblCounter, theme->COUNTER.BORDER_WIDTH, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_lblCounter, theme->COUNTER.PADDING_AROUND, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_lblCounter, theme->COUNTER.PADDING_AROUND, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_lblCounter, theme->COUNTER.PADDING_AROUND, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_lblCounter, theme->COUNTER.PADDING_AROUND, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_lblCounter, theme->COUNTER.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_y(ui_lblCounter, theme->COUNTER.PADDING_TOP);

    switch (theme->COUNTER.ALIGNMENT) {
        case 1:
            lv_obj_set_align(ui_lblCounter, LV_ALIGN_TOP_MID);
            break;
        case 2:
            lv_obj_set_align(ui_lblCounter, LV_ALIGN_TOP_RIGHT);
            lv_obj_set_x(ui_lblCounter, -theme->COUNTER.PADDING_SIDE);
            break;
        default:
            lv_obj_set_align(ui_lblCounter, LV_ALIGN_TOP_LEFT);
            lv_obj_set_x(ui_lblCounter, theme->COUNTER.PADDING_SIDE);
            break;
    }

    ui_pnlEntry = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry = lv_textarea_create(ui_pnlEntry);
    lv_obj_set_width(ui_txtEntry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry, 1024);
    lv_textarea_set_one_line(ui_txtEntry, true);
    lv_obj_set_style_radius(ui_txtEntry, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry, lv_color_hex(theme->OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
