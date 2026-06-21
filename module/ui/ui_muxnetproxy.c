#include "ui_muxshare.h"
#include "ui_muxnetproxy.h"
#include "../../common/device.h"

#define PROXY(NAME, ENUM, UDATA)         \
    lv_obj_t *ui_pnl##NAME##_proxy;      \
    lv_obj_t *ui_lbl##NAME##_proxy;      \
    lv_obj_t *ui_ico##NAME##_proxy;      \
    lv_obj_t *ui_lbl##NAME##Value_proxy;

PROXY_ELEMENTS
#undef PROXY

lv_obj_t *ui_pnlEntry_proxy;
lv_obj_t *ui_txtEntry_proxy;

void init_muxnetproxy(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
#define PROXY(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(proxy, NAME);
    PROXY_ELEMENTS
#undef PROXY

    ui_pnlEntry_proxy = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_proxy, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_proxy, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_proxy, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_proxy, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_proxy, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_proxy, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_proxy, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_proxy, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_proxy, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_proxy, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_proxy, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_proxy, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txtEntry_proxy = lv_textarea_create(ui_pnlEntry_proxy);
    lv_obj_set_width(ui_txtEntry_proxy, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_proxy, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_proxy, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_proxy, OSK_MAX);
    lv_textarea_set_one_line(ui_txtEntry_proxy, true);
    lv_obj_set_style_radius(ui_txtEntry_proxy, theme->OSK.RADIUS, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_proxy, lv_color_hex(theme->OSK.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_proxy, theme->OSK.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_proxy, 2, MU_OBJ_MAIN_DEFAULT);
}
