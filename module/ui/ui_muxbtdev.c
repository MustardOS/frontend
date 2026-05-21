#include "ui_muxshare.h"
#include "ui_muxbtdev.h"
#include "../../common/device.h"

#define BTDEV_INFO(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_btdev;      \
    lv_obj_t *ui_lbl##NAME##_btdev;      \
    lv_obj_t *ui_ico##NAME##_btdev;      \
    lv_obj_t *ui_lbl##NAME##Value_btdev;

BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, ENUM, UDATA) \
    lv_obj_t *ui_pnl##NAME##_btdev;  \
    lv_obj_t *ui_lbl##NAME##_btdev;  \
    lv_obj_t *ui_ico##NAME##_btdev;  \
    lv_obj_t *ui_dro##NAME##_btdev;

BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT

lv_obj_t *ui_pnlEntry_btdev;
lv_obj_t *ui_txtEntry_btdev;

void init_muxbtdev(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
#define BTDEV_INFO(NAME, ENUM, UDATA) CREATE_VALUE_ITEM(btdev, NAME);
    BTDEV_INFO_ELEMENTS
#undef BTDEV_INFO

#define BTDEV_ACT(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(btdev, NAME);
    BTDEV_ACT_ELEMENTS
#undef BTDEV_ACT

    ui_pnlEntry_btdev = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry_btdev, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry_btdev, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry_btdev, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry_btdev, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry_btdev, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry_btdev, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry_btdev, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry_btdev, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry_btdev, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry_btdev, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry_btdev, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry_btdev, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txtEntry_btdev = lv_textarea_create(ui_pnlEntry_btdev);
    lv_obj_set_width(ui_txtEntry_btdev, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry_btdev, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry_btdev, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry_btdev, OSK_MAX);
    lv_textarea_set_one_line(ui_txtEntry_btdev, true);
    lv_obj_set_style_radius(ui_txtEntry_btdev, theme->OSK.RADIUS, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry_btdev, lv_color_hex(theme->OSK.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry_btdev, theme->OSK.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry_btdev, 2, MU_OBJ_MAIN_DEFAULT);
}
