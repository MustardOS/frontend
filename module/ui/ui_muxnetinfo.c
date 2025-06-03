#include "ui_muxshare.h"
#include "ui_muxnetinfo.h"
#include "../../common/device.h"

#define NETINFO(NAME, UDATA)               \
    lv_obj_t *ui_pnl##NAME##_netinfo;      \
    lv_obj_t *ui_lbl##NAME##_netinfo;      \
    lv_obj_t *ui_ico##NAME##_netinfo;      \
    lv_obj_t *ui_lbl##NAME##Value_netinfo;

NETINFO_ELEMENTS
#undef NETINFO

lv_obj_t *ui_pnlEntry_netinfo;
lv_obj_t *ui_txtEntry_netinfo;

void init_muxnetinfo(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
#define NETINFO(NAME, UDATA) CREATE_VALUE_ITEM(netinfo, NAME);
    NETINFO_ELEMENTS
#undef NETINFO

    ui_pnlEntry_netinfo = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_netinfo, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_netinfo, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_netinfo, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_netinfo, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_netinfo, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_netinfo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_netinfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_netinfo, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_netinfo, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_netinfo, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_netinfo, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry_netinfo = lv_textarea_create(ui_pnlEntry_netinfo);
    lv_obj_set_width(ui_txtEntry_netinfo, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_netinfo, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_netinfo, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_netinfo, 1024);
    lv_textarea_set_one_line(ui_txtEntry_netinfo, true);
    lv_obj_set_style_radius(ui_txtEntry_netinfo, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_netinfo, lv_color_hex(theme->OSK.BORDER),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_netinfo, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_netinfo, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
