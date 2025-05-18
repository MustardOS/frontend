#include "ui_muxcharge.h"
#include "../../common/common.h"
#include "../../common/ui_common.h"
#include "../../common/device.h"
#include "../../common/theme.h"
#include "../../common/img/nothing.h"

lv_obj_t *ui_scrCharge_charge;
lv_obj_t *ui_blank_charge;

lv_obj_t *ui_imgWall_charge;

lv_obj_t *ui_pnlWall_charge;
lv_obj_t *ui_pnlCharge_charge;

lv_obj_t *ui_lblCapacity_charge;
lv_obj_t *ui_lblVoltage_charge;
lv_obj_t *ui_lblBoot_charge;

void init_muxcharge(void) {
    ui_scrCharge_charge = lv_obj_create(NULL);

    ui_blank_charge = lv_obj_create(ui_scrCharge_charge);
    lv_obj_set_width(ui_blank_charge, device.MUX.WIDTH);
    lv_obj_set_height(ui_blank_charge, device.MUX.HEIGHT);
    lv_obj_set_align(ui_blank_charge, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_blank_charge, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_blank_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    apply_gradient_to_ui_screen(ui_scrCharge_charge, &theme, &device);

    lv_obj_clear_flag(ui_scrCharge_charge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE |
                                           LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(ui_scrCharge_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_scrCharge_charge, lv_color_hex(theme.SYSTEM.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrCharge_charge, theme.SYSTEM.BACKGROUND_GRADIENT_DIRECTION == LV_GRAD_DIR_NONE
                                                 ? theme.SYSTEM.BACKGROUND_ALPHA : 0,
                            LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlWall_charge = lv_obj_create(ui_scrCharge_charge);
    lv_obj_set_width(ui_pnlWall_charge, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlWall_charge, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlWall_charge, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlWall_charge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_pnlWall_charge, lv_color_hex(theme.CHARGER.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlWall_charge, theme.CHARGER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlWall_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_imgWall_charge = lv_img_create(ui_pnlWall_charge);
    lv_img_set_src(ui_imgWall_charge, &ui_image_Nothing);
    lv_obj_set_align(ui_imgWall_charge, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgWall_charge, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgWall_charge, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnlCharge_charge = lv_obj_create(ui_scrCharge_charge);
    lv_obj_set_width(ui_pnlCharge_charge, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlCharge_charge, device.MUX.HEIGHT / 4);
    lv_obj_set_x(ui_pnlCharge_charge, 0);
    lv_obj_set_y(ui_pnlCharge_charge, theme.CHARGER.Y_POS);
    lv_obj_set_align(ui_pnlCharge_charge, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlCharge_charge, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnlCharge_charge, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlCharge_charge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlCharge_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlCharge_charge, lv_color_hex(theme.CHARGER.BACKGROUND),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlCharge_charge, theme.CHARGER.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlCharge_charge, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlCharge_charge, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlCharge_charge, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_pnlCharge_charge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblCapacity_charge = lv_label_create(ui_pnlCharge_charge);
    lv_label_set_text(ui_lblCapacity_charge, "");
    lv_obj_set_height(ui_lblCapacity_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblCapacity_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblCapacity_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblCapacity_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lblCapacity_charge, "true");
    lv_obj_clear_flag(ui_lblCapacity_charge,
                      LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                      LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblCapacity_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblCapacity_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblCapacity_charge, lv_color_hex(theme.CHARGER.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblCapacity_charge, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblCapacity_charge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblVoltage_charge = lv_label_create(ui_pnlCharge_charge);
    lv_label_set_text(ui_lblVoltage_charge, "");
    lv_obj_set_height(ui_lblVoltage_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblVoltage_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblVoltage_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblVoltage_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lblVoltage_charge, "true");
    lv_obj_clear_flag(ui_lblVoltage_charge,
                      LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                      LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblVoltage_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblVoltage_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblVoltage_charge, lv_color_hex(theme.CHARGER.TEXT),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblVoltage_charge, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblVoltage_charge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblBoot_charge = lv_label_create(ui_pnlCharge_charge);
    lv_label_set_text(ui_lblBoot_charge, "");
    lv_obj_set_height(ui_lblBoot_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lblBoot_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lblBoot_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lblBoot_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lblBoot_charge, "true");
    lv_obj_clear_flag(ui_lblBoot_charge,
                      LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC |
                      LV_OBJ_FLAG_SCROLL_MOMENTUM |
                      LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_scrollbar_mode(ui_lblBoot_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblBoot_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblBoot_charge, lv_color_hex(theme.CHARGER.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblBoot_charge, theme.CHARGER.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblBoot_charge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_disp_load_scr(ui_scrCharge_charge);
}
