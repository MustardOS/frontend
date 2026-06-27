#include "ui_muxmessage.h"
#include "../../common/device.h"
#include "../../common/theme.h"
#include "../../common/ui/common.h"

lv_obj_t *ui_scr_message;

lv_obj_t *ui_img_wall;

lv_obj_t *ui_pnl_wall;

lv_obj_t *ui_pnl_message;

lv_obj_t *ui_lbl_message;

lv_obj_t *ui_bar_progress;

void init_muxmessage(void) {
    ui_scr_message = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(ui_scr_message, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_scr_message, lv_color_hex(theme.verbose_boot.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scr_message, theme.verbose_boot.background_alpha, MU_OBJ_MAIN_DEFAULT);

    ui_pnl_wall = lv_obj_create(ui_scr_message);
    lv_obj_set_width(ui_pnl_wall, device.mux.width);
    lv_obj_set_height(ui_pnl_wall, device.mux.height);
    lv_obj_set_align(ui_pnl_wall, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_wall, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_wall, lv_color_hex(theme.verbose_boot.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_wall, theme.verbose_boot.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_wall, 0, MU_OBJ_MAIN_DEFAULT);

    ui_img_wall = lv_img_create(ui_pnl_wall);
    lv_img_set_src(ui_img_wall, &ui_img_blank);
    lv_obj_set_align(ui_img_wall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_img_wall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_wall, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnl_message = lv_obj_create(ui_scr_message);
    lv_obj_set_width(ui_pnl_message, device.mux.width);
    lv_obj_set_height(ui_pnl_message, device.mux.height / 3);
    lv_obj_set_x(ui_pnl_message, 0);
    lv_obj_set_y(ui_pnl_message, theme.verbose_boot.y_pos);
    lv_obj_set_align(ui_pnl_message, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnl_message, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_pnl_message, lv_color_hex(theme.verbose_boot.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_message, theme.verbose_boot.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(ui_pnl_message, LV_BORDER_SIDE_NONE, MU_OBJ_MAIN_DEFAULT);

    ui_lbl_message = lv_label_create(ui_pnl_message);
    lv_label_set_text(ui_lbl_message, "");
    lv_obj_set_width(ui_lbl_message, device.mux.width - 25);
    lv_obj_set_height(ui_lbl_message, device.mux.height / 3);
    lv_obj_set_align(ui_lbl_message, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lbl_message, "");
    lv_obj_set_scrollbar_mode(ui_lbl_message, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lbl_message, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lbl_message, lv_color_hex(theme.verbose_boot.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(ui_lbl_message, theme.verbose_boot.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_message, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    ui_bar_progress = lv_bar_create(ui_scr_message);
    lv_bar_set_range(ui_bar_progress, 0, 100);
    lv_obj_set_y(ui_bar_progress, 0);
    lv_obj_set_size(ui_bar_progress, device.mux.width, device.mux.height <= 480 ? 12 : 24);
    lv_obj_set_style_pad_all(ui_bar_progress, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(ui_bar_progress, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_bar_progress, lv_color_hex(theme.verbose_boot.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_progress, 25, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_bar_progress, lv_color_hex(theme.verbose_boot.text), MU_OBJ_INDI_DEFAULT);
    lv_obj_set_style_bg_opa(ui_bar_progress, 255, MU_OBJ_INDI_DEFAULT);

    lv_disp_load_scr(ui_scr_message);
}
