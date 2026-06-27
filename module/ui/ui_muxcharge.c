#include "ui_muxcharge.h"
#include "../../common/init.h"
#include "../../common/ui/common.h"
#include "../../common/device.h"
#include "../../common/theme.h"

lv_obj_t *ui_scr_charge_charge;
lv_obj_t *ui_blank_charge;

lv_obj_t *ui_img_wall_charge;

lv_obj_t *ui_pnl_wall_charge;
lv_obj_t *ui_pnl_charge_charge;

lv_obj_t *ui_lbl_capacity_charge;
lv_obj_t *ui_lbl_voltage_charge;
lv_obj_t *ui_lbl_boot_charge;

void init_muxcharge(void) {
    ui_scr_charge_charge = lv_obj_create(NULL);

    ui_blank_charge = lv_obj_create(ui_scr_charge_charge);
    lv_obj_set_width(ui_blank_charge, device.mux.width);
    lv_obj_set_height(ui_blank_charge, device.mux.height);
    lv_obj_set_align(ui_blank_charge, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_blank_charge, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_blank_charge, 0, MU_OBJ_MAIN_DEFAULT);

    apply_gradient_to_ui_screen(ui_scr_charge_charge, &theme, &device);

    lv_obj_clear_flag(
        ui_scr_charge_charge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE
                                  | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM
    );
    lv_obj_set_scrollbar_mode(ui_scr_charge_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_scr_charge_charge, lv_color_hex(theme.system.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(
        ui_scr_charge_charge,
        theme.system.background_gradient_direction == LV_GRAD_DIR_NONE ? theme.system.background_alpha : 0,
        MU_OBJ_MAIN_DEFAULT
    );

    ui_pnl_wall_charge = lv_obj_create(ui_scr_charge_charge);
    lv_obj_set_width(ui_pnl_wall_charge, device.mux.width);
    lv_obj_set_height(ui_pnl_wall_charge, device.mux.height);
    lv_obj_set_align(ui_pnl_wall_charge, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_wall_charge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_pnl_wall_charge, lv_color_hex(theme.charger.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_wall_charge, theme.charger.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_wall_charge, 0, MU_OBJ_MAIN_DEFAULT);

    ui_img_wall_charge = lv_img_create(ui_pnl_wall_charge);
    lv_img_set_src(ui_img_wall_charge, &ui_img_blank);
    lv_obj_set_align(ui_img_wall_charge, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_img_wall_charge, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_wall_charge, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnl_charge_charge = lv_obj_create(ui_scr_charge_charge);
    lv_obj_set_width(ui_pnl_charge_charge, device.mux.width);
    lv_obj_set_height(ui_pnl_charge_charge, device.mux.height / 4);
    lv_obj_set_x(ui_pnl_charge_charge, 0);
    lv_obj_set_y(ui_pnl_charge_charge, theme.charger.y_pos);
    lv_obj_set_align(ui_pnl_charge_charge, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_charge_charge, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(ui_pnl_charge_charge, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_charge_charge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_charge_charge, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_charge_charge, lv_color_hex(theme.charger.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_charge_charge, theme.charger.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_charge_charge, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_charge_charge, 15, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_charge_charge, 30, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_pnl_charge_charge, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_capacity_charge = lv_label_create(ui_pnl_charge_charge);
    lv_label_set_text(ui_lbl_capacity_charge, "");
    lv_obj_set_height(ui_lbl_capacity_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lbl_capacity_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_capacity_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lbl_capacity_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lbl_capacity_charge, 1);
    lv_obj_clear_flag(
        ui_lbl_capacity_charge, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE
                                    | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC
                                    | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN
    );
    lv_obj_set_scrollbar_mode(ui_lbl_capacity_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lbl_capacity_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lbl_capacity_charge, lv_color_hex(theme.charger.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_capacity_charge, theme.charger.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_capacity_charge, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_voltage_charge = lv_label_create(ui_pnl_charge_charge);
    lv_label_set_text(ui_lbl_voltage_charge, "");
    lv_obj_set_height(ui_lbl_voltage_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lbl_voltage_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_voltage_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lbl_voltage_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lbl_voltage_charge, 1);
    lv_obj_clear_flag(
        ui_lbl_voltage_charge, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE
                                   | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC
                                   | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN
    );
    lv_obj_set_scrollbar_mode(ui_lbl_voltage_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lbl_voltage_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lbl_voltage_charge, lv_color_hex(theme.charger.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_voltage_charge, theme.charger.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_voltage_charge, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_boot_charge = lv_label_create(ui_pnl_charge_charge);
    lv_label_set_text(ui_lbl_boot_charge, "");
    lv_obj_set_height(ui_lbl_boot_charge, LV_SIZE_CONTENT);
    lv_obj_set_width(ui_lbl_boot_charge, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_lbl_boot_charge, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_lbl_boot_charge, LV_LABEL_LONG_CLIP);
    lv_label_set_recolor(ui_lbl_boot_charge, 1);
    lv_obj_clear_flag(
        ui_lbl_boot_charge, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE
                                | LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC
                                | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_CHAIN
    );
    lv_obj_set_scrollbar_mode(ui_lbl_boot_charge, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lbl_boot_charge, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lbl_boot_charge, lv_color_hex(theme.charger.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_boot_charge, theme.charger.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_boot_charge, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    lv_disp_load_scr(ui_scr_charge_charge);
}
