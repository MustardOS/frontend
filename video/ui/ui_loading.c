#include "ui_loading.h"

#include <common/display/theme.h>
#include <common/platform/display.h>
#include <common/ui/common.h>
#include <common/ui/font.h>
#include <lvgl/lvgl.h>

static lv_obj_t *loading_dim;
static lv_obj_t *loading_panel;
static lv_obj_t *loading_label;
static lv_obj_t *loading_separator;
static lv_obj_t *loading_detail;

static void style_plain(lv_obj_t *object) {
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(object, 0, MU_OBJ_MAIN_DEFAULT);
}

void video_loading_prepare_content(void) {
    if (!ui_screen || !lv_obj_is_valid(ui_screen)) return;
    lv_obj_set_style_bg_opa(ui_screen, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    set_gradient_visible(0);
    if (ui_pnl_wall && lv_obj_is_valid(ui_pnl_wall)) lv_obj_add_flag(ui_pnl_wall, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_header && lv_obj_is_valid(ui_pnl_header)) lv_obj_add_flag(ui_pnl_header, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_content && lv_obj_is_valid(ui_pnl_content)) lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    if (ui_pnl_footer && lv_obj_is_valid(ui_pnl_footer)) lv_obj_add_flag(ui_pnl_footer, LV_OBJ_FLAG_HIDDEN);
    display_set_ui_hidden(0);
}

static void video_loading_show_detail_alpha(const char *message, const char *detail, const lv_opa_t background_alpha) {
    if (!message || !message[0] || !ui_screen || !lv_obj_is_valid(ui_screen)) return;
    if (loading_label && lv_obj_is_valid(loading_label)) {
        lv_label_set_text(loading_label, message);
        lv_obj_set_style_bg_opa(loading_dim, background_alpha, MU_OBJ_MAIN_DEFAULT);
        if (detail && detail[0]) {
            lv_label_set_text(loading_detail, detail);
            lv_obj_clear_flag(loading_separator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(loading_detail, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(loading_separator, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(loading_detail, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_move_foreground(loading_dim);
        lv_obj_move_foreground(loading_panel);
        lv_obj_invalidate(ui_screen);
        lv_refr_now(NULL);
        display_composite_frame();
        return;
    }

    loading_dim = lv_obj_create(ui_screen);
    style_plain(loading_dim);
    lv_obj_set_size(loading_dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(loading_dim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(loading_dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_dim, background_alpha, MU_OBJ_MAIN_DEFAULT);

    loading_panel = lv_obj_create(ui_screen);
    lv_obj_clear_flag(loading_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(loading_panel, lv_pct(70));
    lv_obj_set_height(loading_panel, LV_SIZE_CONTENT);
    lv_obj_align(loading_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(loading_panel, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_panel, theme.dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(loading_panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(loading_panel, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(loading_panel, theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(loading_panel, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(loading_panel, 18, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(loading_panel, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(loading_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(loading_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    loading_label = lv_label_create(loading_panel);
    lv_label_set_text(loading_label, message);
    lv_label_set_long_mode(loading_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(loading_label, LV_PCT(100));
    lv_obj_set_style_text_align(loading_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(loading_label, lv_color_hex(theme.dialogue.content), MU_OBJ_MAIN_DEFAULT);

    loading_separator = lv_obj_create(loading_panel);
    style_plain(loading_separator);
    lv_obj_set_size(loading_separator, lv_pct(45), 1);
    lv_obj_set_style_bg_color(loading_separator, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_separator, theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);

    loading_detail = lv_label_create(loading_panel);
    lv_label_set_long_mode(loading_detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(loading_detail, LV_PCT(100));
    lv_obj_set_style_text_align(loading_detail, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(loading_detail, lv_color_hex(theme.dialogue.content), MU_OBJ_MAIN_DEFAULT);
    if (detail && detail[0]) {
        lv_label_set_text(loading_detail, detail);
    } else {
        lv_obj_add_flag(loading_separator, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(loading_detail, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_move_foreground(loading_dim);
    lv_obj_move_foreground(loading_panel);
    lv_obj_update_layout(ui_screen);
    lv_obj_invalidate(ui_screen);
    lv_refr_now(NULL);
    display_composite_frame();
}

void video_loading_show_detail(const char *message, const char *detail, const int opaque) {
    video_loading_show_detail_alpha(message, detail, opaque ? LV_OPA_COVER : theme.dialogue.dim_alpha);
}

void video_loading_show_transparent(const char *message, const char *detail) {
    video_loading_show_detail_alpha(message, detail, LV_OPA_TRANSP);
}

void video_loading_show(const char *message) {
    video_loading_show_detail(message, NULL, 0);
}

void video_loading_hide(void) {
    if (loading_panel && lv_obj_is_valid(loading_panel)) lv_obj_del(loading_panel);
    if (loading_dim && lv_obj_is_valid(loading_dim)) lv_obj_del(loading_dim);
    loading_panel = NULL;
    loading_dim = NULL;
    loading_label = NULL;
    loading_separator = NULL;
    loading_detail = NULL;
    if (ui_screen && lv_obj_is_valid(ui_screen)) {
        lv_obj_invalidate(ui_screen);
        lv_refr_now(NULL);
    }
}
