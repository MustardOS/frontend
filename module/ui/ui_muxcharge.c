#include "ui_muxcharge.h"
#include "../../common/common.h"
#include "../../common/device.h"
#include "../../common/theme.h"
#include "../../font/notosans.h"
#include "../../common/img/nothing.h"

lv_obj_t *ui_scrCharge;

lv_obj_t *ui_imgWall;

lv_obj_t *ui_pnlWall;
lv_obj_t *ui_pnlCharge;

lv_obj_t *ui_lblCapacity;
lv_obj_t *ui_lblVoltage;
lv_obj_t *ui_lblBoot;

void ui_init(void) {
    ui_scrCharge = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scrCharge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE |
                                    LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(ui_scrCharge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_scrCharge, lv_color_hex(theme.CHARGER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrCharge, theme.CHARGER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_scrCharge, &ui_font_NotoSans, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlWall = lv_obj_create(ui_scrCharge);
    lv_obj_set_width(ui_pnlWall, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlWall, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlWall, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlWall, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_pnlWall, lv_color_hex(theme.CHARGER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlWall, theme.CHARGER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_imgWall = lv_img_create(ui_pnlWall);
    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    lv_obj_set_width(ui_imgWall, device.MUX.WIDTH);
    lv_obj_set_height(ui_imgWall, device.MUX.HEIGHT);
    lv_obj_set_align(ui_imgWall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgWall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgWall, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnlCharge = lv_obj_create(ui_scrCharge);
    lv_obj_set_width(ui_pnlCharge, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlCharge, device.MUX.HEIGHT / 4);
    lv_obj_set_x(ui_pnlCharge, 0);
    lv_obj_set_y(ui_pnlCharge, theme.CHARGER.Y_POS);
    lv_obj_set_align(ui_pnlCharge, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlCharge, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlCharge, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlCharge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlCharge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlCharge, lv_color_hex(theme.CHARGER.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlCharge, theme.CHARGER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlCharge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlCharge, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlCharge, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_pnlCharge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblCapacity = lv_label_create(ui_pnlCharge);
    lv_obj_set_height(ui_lblCapacity, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblCapacity, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCapacity, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblCapacity, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lblCapacity, "true");
    lv_obj_clear_flag(ui_lblCapacity,
                      LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                      LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblCapacity, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblCapacity, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblCapacity, lv_color_hex(theme.CHARGER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCapacity, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCapacity, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblVoltage = lv_label_create(ui_pnlCharge);
    lv_obj_set_height(ui_lblVoltage, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblVoltage, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblVoltage, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblVoltage, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lblVoltage, "true");
    lv_obj_clear_flag(ui_lblVoltage, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                                     LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                                     LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                     LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblVoltage, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblVoltage, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblVoltage, lv_color_hex(theme.CHARGER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblVoltage, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblVoltage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblBoot = lv_label_create(ui_pnlCharge);
    lv_obj_set_height(ui_lblBoot, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblBoot, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblBoot, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblBoot, LV_LABEL_LONG_CLIP);
    lv_label_set_text(ui_lblBoot, TS("Press POWER button to continue booting..."));
    lv_label_set_recolor(ui_lblBoot, "true");
    lv_obj_clear_flag(ui_lblBoot, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                                  LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                                  LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                  LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblBoot, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblBoot, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblBoot, lv_color_hex(theme.CHARGER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBoot, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBoot, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
}
