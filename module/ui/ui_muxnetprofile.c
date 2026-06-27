#include "ui_muxshare.h"
#include "ui_muxnetprofile.h"
#include "../../common/device.h"

#define NETWORK(NAME, UDATA)                                                                                           \
    lv_obj_t *ui_pnl_##NAME##_network;                                                                                 \
    lv_obj_t *ui_lbl_##NAME##_network;                                                                                 \
    lv_obj_t *ui_ico_##NAME##_network;                                                                                 \
    lv_obj_t *ui_val_##NAME##_network;

NETWORK_ELEMENTS
#undef NETWORK

lv_obj_t *ui_pnl_entry_network;
lv_obj_t *ui_txt_entry_network;

void init_muxnetwork(lv_obj_t *ui_screen, lv_obj_t *ui_pnl_content, const struct theme_config *theme) {
#define NETWORK(NAME, UDATA) CREATE_VALUE_ITEM(network, NAME);
    NETWORK_ELEMENTS
#undef NETWORK

    ui_pnl_entry_network = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_network, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_network, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_network, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_network, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_network, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_network, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_network, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_network, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_network, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_network, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_network, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_network, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_network = lv_textarea_create(ui_pnl_entry_network);
    lv_obj_set_width(ui_txt_entry_network, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_network, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_network, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_network, 1024);
    lv_textarea_set_one_line(ui_txt_entry_network, 1);
    lv_obj_set_style_radius(ui_txt_entry_network, theme->osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_network, lv_color_hex(theme->osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_network, theme->osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_network, 2, MU_OBJ_MAIN_DEFAULT);
}
