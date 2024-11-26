#include "ui.h"
#include "../../common/common.h"
#include "../../common/device.h"
#include "../../common/theme.h"
#include "../../font/gamepad.h"

void ui_scrTester_screen_init(lv_obj_t *ui_pnlContent) {
    lv_obj_set_flex_align(ui_pnlContent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_lblButton = lv_label_create(ui_pnlContent);
    lv_obj_set_width(ui_lblButton, device.MUX.WIDTH);
    lv_obj_set_height(ui_lblButton, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblButton, 0);
    lv_obj_set_y(ui_lblButton, device.MUX.HEIGHT / 2);
    lv_obj_set_align(ui_lblButton, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblButton, "");
    lv_obj_add_flag(ui_lblButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(ui_lblButton, lv_color_hex(theme.LIST_DEFAULT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblButton, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblButton, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lblButton, &ui_font_Gamepad, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblFirst = lv_label_create(ui_pnlContent);
    lv_obj_set_width(ui_lblFirst, device.MUX.WIDTH);
    lv_obj_set_height(ui_lblFirst, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_lblFirst, 0);
    lv_obj_set_y(ui_lblFirst, device.MUX.HEIGHT / 2);
    lv_obj_set_align(ui_lblFirst, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblFirst, TS("Press any button to start input testing!"));
    lv_obj_set_style_text_color(ui_lblFirst, lv_color_hex(theme.LIST_DEFAULT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblFirst, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblFirst, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
}
