#include "ui_muxmessage.h"
#include "../../common/device.h"
#include "../../common/theme.h"
#include "../../common/img/nothing.h"

lv_obj_t *ui_scrMessage;

lv_obj_t *ui_imgWall;

lv_obj_t *ui_pnlWall;

lv_obj_t *ui_pnlMessage;

lv_obj_t *ui_lblMessage;

lv_obj_t *ui_barProgress;

void init_muxmessage(void) {
    ui_scrMessage = lv_obj_create(NULL);
    lv_obj_set_scrollbar_mode(ui_scrMessage, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui_scrMessage, lv_color_hex(theme.VERBOSE_BOOT.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scrMessage, theme.VERBOSE_BOOT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_pnlWall = lv_obj_create(ui_scrMessage);
    lv_obj_set_width(ui_pnlWall, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlWall, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlWall, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlWall, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlWall, lv_color_hex(theme.VERBOSE_BOOT.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlWall, theme.VERBOSE_BOOT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlWall, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_imgWall = lv_img_create(ui_pnlWall);
    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    lv_obj_set_align(ui_imgWall, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_imgWall, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_imgWall, LV_OBJ_FLAG_SCROLLABLE);

    ui_pnlMessage = lv_obj_create(ui_scrMessage);
    lv_obj_set_width(ui_pnlMessage, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlMessage, device.MUX.HEIGHT / 3);
    lv_obj_set_x(ui_pnlMessage, 0);
    lv_obj_set_y(ui_pnlMessage, theme.VERBOSE_BOOT.Y_POS);
    lv_obj_set_align(ui_pnlMessage, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_pnlMessage, lv_color_hex(theme.VERBOSE_BOOT.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlMessage, theme.VERBOSE_BOOT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_pnlMessage, LV_BORDER_SIDE_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lblMessage = lv_label_create(ui_pnlMessage);
    lv_label_set_text(ui_lblMessage, "");
    lv_obj_set_width(ui_lblMessage, device.MUX.WIDTH - 25);
    lv_obj_set_height(ui_lblMessage, device.MUX.HEIGHT / 3);
    lv_obj_set_align(ui_lblMessage, LV_ALIGN_CENTER);
    lv_label_set_text(ui_lblMessage, "");
    lv_obj_set_scrollbar_mode(ui_lblMessage, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(ui_lblMessage, LV_DIR_LEFT);
    lv_obj_set_style_text_color(ui_lblMessage, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_lblMessage, theme.VERBOSE_BOOT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lblMessage, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_barProgress = lv_bar_create(ui_scrMessage);
    lv_bar_set_range(ui_barProgress, 0, 100);
    lv_obj_set_y(ui_barProgress, 0);
    lv_obj_set_size(ui_barProgress, device.MUX.WIDTH, 24);
    lv_obj_set_style_pad_all(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_barProgress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgress, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_barProgress, lv_color_hex(theme.VERBOSE_BOOT.TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_barProgress, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_disp_load_scr(ui_scrMessage);
}
