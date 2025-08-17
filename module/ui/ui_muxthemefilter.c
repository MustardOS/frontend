#include "ui_muxshare.h"
#include "ui_muxthemefilter.h"
#include "../../common/device.h"

#define THEMEFILTER(NAME, UDATA)          \
    lv_obj_t *ui_pnl##NAME##_themefilter; \
    lv_obj_t *ui_lbl##NAME##_themefilter; \
    lv_obj_t *ui_ico##NAME##_themefilter; \
    lv_obj_t *ui_dro##NAME##_themefilter;

THEMEFILTER_ELEMENTS
#undef THEMEFILTER

lv_obj_t *ui_pnlLookup_themefilter;
lv_obj_t *ui_lblLookup_themefilter;
lv_obj_t *ui_icoLookup_themefilter;
lv_obj_t *ui_lblLookupValue_themefilter;
lv_obj_t *ui_pnlEntry_themefilter;
lv_obj_t *ui_txtEntry_themefilter;

void init_muxthemefilter(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
#define THEMEFILTER(NAME, UDATA) CREATE_OPTION_ITEM(themefilter, NAME);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER

    CREATE_VALUE_ITEM(themefilter, Lookup);

    ui_pnlEntry_themefilter = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_themefilter, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_themefilter, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_themefilter, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_themefilter, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_themefilter, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_themefilter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_themefilter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_themefilter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_themefilter, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_themefilter, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_themefilter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_themefilter, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry_themefilter = lv_textarea_create(ui_pnlEntry_themefilter);
    lv_obj_set_width(ui_txtEntry_themefilter, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_themefilter, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_themefilter, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_themefilter, 1024);
    lv_textarea_set_one_line(ui_txtEntry_themefilter, true);
    lv_obj_set_style_radius(ui_txtEntry_themefilter, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_themefilter, lv_color_hex(theme->OSK.BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_themefilter, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_themefilter, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
