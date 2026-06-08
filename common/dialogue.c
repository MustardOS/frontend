#include "dialogue.h"
#include "config.h"
#include "transition.h"
#include "language.h"
#include "ui_common.h"

static void panel_anim_y_cb(void *obj, int32_t y) {
    lv_obj_set_y((lv_obj_t *) obj, (lv_coord_t) y);
}

static void panel_anim_x_cb(void *obj, int32_t x) {
    lv_obj_set_style_translate_x((lv_obj_t *) obj, (lv_coord_t) x, MU_OBJ_MAIN_DEFAULT);
}

static void panel_anim_opa_cb(void *obj, int32_t opa) {
    lv_obj_set_style_opa((lv_obj_t *) obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void dim_anim_opa_cb(void *obj, int32_t opa) {
    lv_obj_set_style_bg_opa((lv_obj_t *) obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void hide_anim_ready_cb(lv_anim_t *a) {
    mux_dialogue *dlg = (mux_dialogue *) lv_anim_get_user_data(a);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
}

void dialogue_init(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                   const char *title, const char *description, const char **options, int option_count,
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
    dlg->dim_alpha = t->DIALOGUE.DIM_ALPHA;

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

    dlg->description_label = NULL;
    if (description) {
        dlg->description_label = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->description_label, description);
        lv_obj_set_width(dlg->description_label, (LV_HOR_RES * 60 / 100) - 32);
        lv_obj_set_style_text_align(dlg->description_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->description_label, lv_color_hex(t->DIALOGUE.CONTENT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->description_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(dlg->description_label, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(dlg->description_label, 8, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_long_mode(dlg->description_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *sep_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_top, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(t->DIALOGUE.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_top, t->DIALOGUE.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(sep_top, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *gap_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(gap_top, LV_PCT(100), 8);
    lv_obj_clear_flag(gap_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gap_top, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(gap_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(gap_top, 0, MU_OBJ_MAIN_DEFAULT);

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

    lv_obj_t *gap_bot = lv_obj_create(dlg->panel);
    lv_obj_set_size(gap_bot, LV_PCT(100), 8);
    lv_obj_clear_flag(gap_bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gap_bot, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(gap_bot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(gap_bot, 0, MU_OBJ_MAIN_DEFAULT);

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

void dialogue_init_unsaved(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                           const char *title, const char *description, const char *save_label, const char *discard_label,
                           const char *nav_a, const char *nav_b) {
    const char *opts[MUX_UNSAVED_NOPE] = {save_label, discard_label};
    dialogue_init(dlg, t, parent, title, description, opts, MUX_UNSAVED_NOPE, nav_a, nav_b);
}

void dialogue_init_confirm(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                           const char *title, const char *description, const char *confirm_label, const char *cancel_label,
                           const char *nav_a, const char *nav_b) {
    const char *opts[MUX_CONFIRM_CNT] = {confirm_label, cancel_label};
    dialogue_init(dlg, t, parent, title, description, opts, MUX_CONFIRM_CNT, nav_a, nav_b);
}

void dialogue_init_warn(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                        const char *description, const char *nav_a, const char *nav_b) {
    const char *opts[] = {lang.GENERIC.UNDERSTAND, lang.GENERIC.CANCEL};
    dialogue_init(dlg, t, parent, lang.GENERIC.WARNING, description, opts, 2, nav_a, nav_b);
}

void dialogue_init_remove(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                          const char *description, const char *nav_a, const char *nav_b) {
    const char *opts[MUX_REMOVE_CNT] = {lang.GENERIC.REMOVE, lang.GENERIC.SKIP_CONFIRM, lang.GENERIC.CANCEL};
    dialogue_init(dlg, t, parent, lang.GENERIC.CONFIRM, description, opts, MUX_REMOVE_CNT, nav_a, nav_b);
}

void dialogue_init_message(mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent,
                           const char *title, const char *description, const char *message, const char *nav_b) {
    dlg->option_count = 0;
    dlg->selected = 0;

    dlg->dim = lv_obj_create(parent);
    lv_obj_set_size(dlg->dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dlg->dim, 0, 0);
    lv_obj_set_style_bg_color(dlg->dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->dim, t->DIALOGUE.DIM_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    dlg->dim_alpha = t->DIALOGUE.DIM_ALPHA;

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

    dlg->description_label = NULL;
    if (description) {
        dlg->description_label = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->description_label, description);
        lv_obj_set_width(dlg->description_label, (LV_HOR_RES * 60 / 100) - 32);
        lv_obj_set_style_text_align(dlg->description_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->description_label, lv_color_hex(t->DIALOGUE.CONTENT), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->description_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(dlg->description_label, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(dlg->description_label, 8, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_long_mode(dlg->description_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *sep_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_top, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(t->DIALOGUE.BORDER), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_top, t->DIALOGUE.BORDER_ALPHA, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(sep_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(sep_top, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *gap_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(gap_top, LV_PCT(100), 8);
    lv_obj_clear_flag(gap_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gap_top, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(gap_top, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(gap_top, 0, MU_OBJ_MAIN_DEFAULT);

    dlg->options[0] = lv_label_create(dlg->panel);
    lv_label_set_text(dlg->options[0], message);
    lv_obj_set_width(dlg->options[0], (LV_HOR_RES * 60 / 100) - 32);
    lv_obj_set_style_text_align(dlg->options[0], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->options[0], lv_color_hex(t->DIALOGUE.CONTENT), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->options[0], LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->options[0], 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_ver(dlg->options[0], 6, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_long_mode(dlg->options[0], LV_LABEL_LONG_WRAP);

    lv_obj_t *gap_bot = lv_obj_create(dlg->panel);
    lv_obj_set_size(gap_bot, LV_PCT(100), 8);
    lv_obj_clear_flag(gap_bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gap_bot, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(gap_bot, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(gap_bot, 0, MU_OBJ_MAIN_DEFAULT);

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

    lv_obj_t *glyph_b = create_footer_glyph(footer_row, t, "b", t->NAV.B, 0);
    lv_obj_t *label_b = create_footer_text(footer_row, t, t->NAV.B.TEXT, t->NAV.B.TEXT_ALPHA, 0);
    lv_label_set_text(label_b, nav_b);

    (void) glyph_b;
}

void dialogue_show(mux_dialogue *dlg) {
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_y_cb);
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_x_cb);
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_opa_cb);
    lv_anim_del(dlg->dim, (lv_anim_exec_xcb_t) dim_anim_opa_cb);
    lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_move_foreground(dlg->dim);
    lv_obj_move_foreground(dlg->panel);

    lv_obj_set_style_bg_opa(dlg->dim, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_clear_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_update_layout(dlg->panel);
    lv_coord_t panel_h = lv_obj_get_height(dlg->panel);
    lv_coord_t target_y = (LV_VER_RES - panel_h) / 2;

    lv_obj_set_style_align(dlg->panel, LV_ALIGN_TOP_MID, 0);

    int diag_transition = config.VISUAL.DIALOGUETRANSITION;

    lv_anim_path_cb_t path;
    uint32_t duration;
    switch (diag_transition) {
        case TSN_BOUNCE_RIGHT:
        case TSN_BOUNCE_LEFT:
        case TSN_BOUNCE_UP:
        case TSN_BOUNCE_DOWN:
            path = lv_anim_path_bounce;
            duration = 450;
            break;
        case TSN_SHOOT_RIGHT:
        case TSN_SHOOT_LEFT:
        case TSN_SHOOT_UP:
        case TSN_SHOOT_DOWN:
            path = lv_anim_path_overshoot;
            duration = 350;
            break;
        default:
            path = lv_anim_path_ease_out;
            duration = 250;
            break;
    }

    lv_anim_t ap;
    lv_anim_init(&ap);
    lv_anim_set_var(&ap, dlg->panel);

    switch (diag_transition) {
        case TSN_FADE_IN:
            lv_obj_set_y(dlg->panel, target_y);
            lv_obj_set_style_opa(dlg->panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
            lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_opa_cb);
            lv_anim_set_values(&ap, LV_OPA_TRANSP, LV_OPA_COVER);
            break;
        case TSN_SLIDE_RIGHT:
        case TSN_BOUNCE_RIGHT:
        case TSN_SHOOT_RIGHT:
            lv_obj_set_y(dlg->panel, target_y);
            lv_obj_set_style_translate_x(dlg->panel, LV_HOR_RES, MU_OBJ_MAIN_DEFAULT);
            lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_x_cb);
            lv_anim_set_values(&ap, LV_HOR_RES, 0);
            break;
        case TSN_SLIDE_LEFT:
        case TSN_BOUNCE_LEFT:
        case TSN_SHOOT_LEFT:
            lv_obj_set_y(dlg->panel, target_y);
            lv_obj_set_style_translate_x(dlg->panel, -LV_HOR_RES, MU_OBJ_MAIN_DEFAULT);
            lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_x_cb);
            lv_anim_set_values(&ap, -LV_HOR_RES, 0);
            break;
        case TSN_SLIDE_DOWN:
        case TSN_BOUNCE_DOWN:
        case TSN_SHOOT_DOWN:
            lv_obj_set_y(dlg->panel, -panel_h);
            lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_y_cb);
            lv_anim_set_values(&ap, -panel_h, target_y);
            break;
        case TSN_DISABLED:
            lv_obj_set_y(dlg->panel, target_y);
            goto skip_panel_anim;
        default:
            lv_obj_set_y(dlg->panel, LV_VER_RES);
            lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_y_cb);
            lv_anim_set_values(&ap, LV_VER_RES, target_y);
            break;
    }

    lv_anim_set_time(&ap, duration);
    lv_anim_set_path_cb(&ap, path);
    lv_anim_start(&ap);

    skip_panel_anim:;

    lv_anim_t ad;
    lv_anim_init(&ad);
    lv_anim_set_var(&ad, dlg->dim);
    lv_anim_set_exec_cb(&ad, (lv_anim_exec_xcb_t) dim_anim_opa_cb);
    lv_anim_set_values(&ad, LV_OPA_TRANSP, dlg->dim_alpha);
    lv_anim_set_time(&ad, 200);
    lv_anim_set_path_cb(&ad, lv_anim_path_linear);
    lv_anim_start(&ad);
}

void dialogue_hide(mux_dialogue *dlg) {
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_y_cb);
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_x_cb);
    lv_anim_del(dlg->panel, (lv_anim_exec_xcb_t) panel_anim_opa_cb);
    lv_anim_del(dlg->dim, (lv_anim_exec_xcb_t) dim_anim_opa_cb);

    lv_anim_t ap;
    lv_anim_init(&ap);
    lv_anim_set_var(&ap, dlg->panel);
    lv_anim_set_exec_cb(&ap, (lv_anim_exec_xcb_t) panel_anim_opa_cb);
    lv_anim_set_values(&ap, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&ap, 150);
    lv_anim_set_path_cb(&ap, lv_anim_path_linear);
    lv_anim_start(&ap);

    lv_anim_t ad;
    lv_anim_init(&ad);
    lv_anim_set_var(&ad, dlg->dim);
    lv_anim_set_exec_cb(&ad, (lv_anim_exec_xcb_t) dim_anim_opa_cb);
    lv_anim_set_values(&ad, dlg->dim_alpha, LV_OPA_TRANSP);
    lv_anim_set_time(&ad, 150);
    lv_anim_set_path_cb(&ad, lv_anim_path_linear);
    lv_anim_set_ready_cb(&ad, hide_anim_ready_cb);
    lv_anim_set_user_data(&ad, dlg);
    lv_anim_start(&ad);
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
