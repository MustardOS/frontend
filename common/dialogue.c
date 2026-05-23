#include "dialogue.h"
#include "ui_common.h"

void dialogue_init(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                   const char *title, const char **options, int option_count,
                   const char *nav_a, const char *nav_b) {
    if (option_count > MUX_DIALOGUE_MAX_OPTIONS) option_count = MUX_DIALOGUE_MAX_OPTIONS;

    dlg->option_count = option_count;
    dlg->selected = 0;

    dlg->dim = lv_obj_create(parent);
    lv_obj_set_size(dlg->dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dlg->dim, 0, 0);
    lv_obj_set_style_bg_color(dlg->dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->dim, t->DIALOGUE.DIM_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);

    dlg->panel = lv_obj_create(parent);
    lv_obj_set_size(dlg->panel, lv_pct(60), LV_SIZE_CONTENT);
    lv_obj_align(dlg->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dlg->panel, lv_color_hex(t->DIALOGUE.BACKGROUND), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->panel, t->DIALOGUE.BACKGROUND_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(dlg->panel, lv_color_hex(t->DIALOGUE.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(dlg->panel, t->DIALOGUE.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->panel, t->DIALOGUE.RADIUS.MAIN, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(dlg->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(dlg->panel, LV_FLEX_ALIGN_START, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);

    dlg->title_label = lv_label_create(dlg->panel);
    lv_label_set_text(dlg->title_label, title);
    lv_obj_set_width(dlg->title_label, (LV_HOR_RES * 60 / 100) - 32);
    lv_obj_set_style_text_align(dlg->title_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->title_label, lv_color_hex(t->DIALOGUE.TITLE), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->title_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->title_label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *sep_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_top, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(t->DIALOGUE.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_top, t->DIALOGUE.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(sep_top, 0, MU_OBJ_MAIN_DEFAULT);

    for (int i = 0; i < option_count; i++) {
        dlg->options[i] = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->options[i], options[i]);
        lv_obj_set_width(dlg->options[i], LV_PCT(100));
        lv_obj_set_style_text_align(dlg->options[i], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_ver(dlg->options[i], 6, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_radius(dlg->options[i], t->DIALOGUE.RADIUS.SELECTED, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->options[i], lv_color_hex(t->DIALOGUE.CONTENT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->options[i], LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    }

    lv_obj_t *sep_bot = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_bot, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_bot, lv_color_hex(t->DIALOGUE.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_bot, t->DIALOGUE.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(sep_bot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(sep_bot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(sep_bot, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *footer_row = lv_obj_create(dlg->panel);
    lv_obj_set_size(footer_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(footer_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(footer_row, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(footer_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(footer_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(footer_row, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(footer_row, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(footer_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(footer_row, LV_FLEX_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_flex_cross_place(footer_row, LV_FLEX_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *glyph_a = create_footer_glyph(footer_row, t, "a", t->NAV.A, 0);
    lv_obj_t *label_a = create_footer_text(footer_row, t, t->NAV.A.TEXT, t->NAV.A.TEXT_ALPHA, 0);
    lv_label_set_text(label_a, nav_a);

    lv_obj_t *glyph_b = create_footer_glyph(footer_row, t, "b", t->NAV.B, 0);
    lv_obj_t *label_b = create_footer_text(footer_row, t, t->NAV.B.TEXT, t->NAV.B.TEXT_ALPHA, 0);
    lv_label_set_text(label_b, nav_b);

    (void) glyph_a;
    (void) glyph_b;
}

void dialogue_show(mux_dialogue *dlg) {
    lv_obj_move_foreground(dlg->dim);
    lv_obj_move_foreground(dlg->panel);

    lv_obj_clear_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
}

void dialogue_hide(mux_dialogue *dlg) {
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
}

void dialogue_navigate(mux_dialogue *dlg, struct theme_config *t, int delta) {
    if (dlg->option_count <= 0) return;

    dlg->selected = (dlg->selected + delta + dlg->option_count) % dlg->option_count;
    dialogue_refresh(dlg, t);
}

void dialogue_refresh(mux_dialogue *dlg, struct theme_config *t) {
    for (int i = 0; i < dlg->option_count; i++) {
        int sel = (i == dlg->selected);
        lv_obj_set_style_text_color(dlg->options[i], lv_color_hex(sel ? t->DIALOGUE.OPTION : t->DIALOGUE.CONTENT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(dlg->options[i], lv_color_hex(t->DIALOGUE.SELECTION), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->options[i], sel ? t->DIALOGUE.SELECTION_ALPHA : LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    }
}
