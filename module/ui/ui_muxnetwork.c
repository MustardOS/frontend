#include "ui_muxshare.h"
#include "ui_muxnetwork.h"
#include "../../common/device.h"

#define NETWORK(NAME, UDATA)               \
    lv_obj_t *ui_pnl##NAME##_network;      \
    lv_obj_t *ui_lbl##NAME##_network;      \
    lv_obj_t *ui_ico##NAME##_network;      \
    lv_obj_t *ui_lbl##NAME##Value_network;

NETWORK_ELEMENTS
#undef NETWORK

lv_obj_t *ui_pnlEntry_network;
lv_obj_t *ui_txtEntry_network;

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
#define NETWORK(NAME, UDATA) CREATE_VALUE_ITEM(network, NAME);
    NETWORK_ELEMENTS
#undef NETWORK

    ui_pnlEntry_network = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_network, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_network, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_network, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_network, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_network, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_network, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_network, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_network, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_network, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_network, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_network, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txtEntry_network = lv_textarea_create(ui_pnlEntry_network);
    lv_obj_set_width(ui_txtEntry_network, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_network, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_network, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_network, 1024);
    lv_textarea_set_one_line(ui_txtEntry_network, true);
    lv_obj_set_style_radius(ui_txtEntry_network, theme->OSK.RADIUS, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_network, lv_color_hex(theme->OSK.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_network, theme->OSK.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_network, 2, MU_OBJ_MAIN_DEFAULT);
}
