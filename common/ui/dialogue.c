#include "dialogue.h"
#include "common.h"
#include "../audio.h"
#include "../config.h"
#include "../core/common.h"
#include "transition.h"
#include "../language.h"
#include "../init.h"

static void dialogue_force_redraw(mux_dialogue *dlg) {
    if (!dlg || !dlg->panel) return;

    lv_obj_update_layout(dlg->panel);

    const lv_obj_t *parent = lv_obj_get_parent(dlg->panel);
    lv_obj_invalidate(parent ? parent : dlg->panel);

    lv_refr_now(NULL);
}

static void panel_anim_y_cb(void *obj, const int32_t y) {
    lv_obj_set_y(obj, y);
}

static void panel_anim_x_cb(void *obj, const int32_t x) {
    lv_obj_set_style_translate_x(obj, x, MU_OBJ_MAIN_DEFAULT);
}

static void panel_anim_opa_cb(void *obj, const int32_t opa) {
    lv_obj_set_style_opa(obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void dim_anim_opa_cb(void *obj, const int32_t opa) {
    lv_obj_set_style_bg_opa(obj, (lv_opa_t) opa, MU_OBJ_MAIN_DEFAULT);
}

static void show_anim_ready_cb(lv_anim_t *a) {
    mux_dialogue *dlg = lv_anim_get_user_data(a);
    if (!dlg) return;

    lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);

    dialogue_force_redraw(dlg);
}

void dialogue_init(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char **options, int option_count, const char *nav_a, const char *nav_b
) {
    if (option_count > MUX_DIALOGUE_MAX_OPTIONS) option_count = MUX_DIALOGUE_MAX_OPTIONS;

    dlg->option_count = option_count;
    dlg->selected = 0;
    dlg->theme = t;

    dlg->dim = lv_obj_create(parent);
    lv_obj_set_size(dlg->dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dlg->dim, 0, 0);
    lv_obj_set_style_bg_color(dlg->dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->dim, t->dialogue.dim_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    dlg->dim_alpha = t->dialogue.dim_alpha;

    dlg->panel = lv_obj_create(parent);
    lv_obj_set_size(dlg->panel, lv_pct(60), LV_SIZE_CONTENT);
    lv_obj_align(dlg->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dlg->panel, lv_color_hex(t->dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->panel, t->dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(dlg->panel, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(dlg->panel, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->panel, t->dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(dlg->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(dlg->panel, LV_FLEX_ALIGN_START, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_shadow_zone_register(
        dlg->panel, lv_color_hex(t->dialogue.shadow_colour), (lv_opa_t) t->dialogue.shadow_alpha,
        (int8_t) t->dialogue.shadow_x_offset, (int8_t) t->dialogue.shadow_y_offset,
        lv_color_hex(t->dialogue.shadow_colour_focus), (lv_opa_t) t->dialogue.shadow_alpha_focus,
        (int8_t) t->dialogue.shadow_x_offset_focus, (int8_t) t->dialogue.shadow_y_offset_focus
    );

    dlg->title_label = lv_label_create(dlg->panel);
    lv_label_set_text(dlg->title_label, title);
    lv_obj_set_width(dlg->title_label, LV_PCT(100));
    lv_obj_set_style_text_align(dlg->title_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->title_label, lv_color_hex(t->dialogue.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->title_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->title_label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);

    dlg->description_label = NULL;
    if (description) {
        dlg->description_label = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->description_label, description);
        lv_obj_set_width(dlg->description_label, LV_PCT(100));
        lv_obj_set_style_text_align(dlg->description_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->description_label, lv_color_hex(t->dialogue.content), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->description_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(dlg->description_label, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(dlg->description_label, 8, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_long_mode(dlg->description_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *sep_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_top, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_top, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
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
        lv_obj_set_style_radius(dlg->options[i], t->dialogue.radius.selected, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->options[i], lv_color_hex(t->dialogue.content), MU_OBJ_MAIN_DEFAULT);
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
    lv_obj_set_style_bg_color(sep_bot, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_bot, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
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

    const lv_obj_t *glyph_a = create_footer_glyph(footer_row, t, "a", t->nav.a, 0);
    lv_obj_t *label_a = create_footer_text(footer_row, t, t->nav.a.text, t->nav.a.text_alpha, 0);
    lv_label_set_text(label_a, nav_a);

    const lv_obj_t *glyph_b = create_footer_glyph(footer_row, t, "b", t->nav.b, 0);
    lv_obj_t *label_b = create_footer_text(footer_row, t, t->nav.b.text, t->nav.b.text_alpha, 0);
    lv_label_set_text(label_b, nav_b);

    (void) glyph_a;
    (void) glyph_b;
}

void dialogue_init_unsaved(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *save_label, const char *discard_label, const char *nav_a, const char *nav_b
) {
    const char *opts[mux_unsaved_nope] = {save_label, discard_label};
    dialogue_init(dlg, t, parent, title, description, opts, mux_unsaved_nope, nav_a, nav_b);
}

void dialogue_init_confirm(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *confirm_label, const char *cancel_label, const char *nav_a, const char *nav_b
) {
    const char *opts[mux_confirm_cnt] = {confirm_label, cancel_label};
    dialogue_init(dlg, t, parent, title, description, opts, mux_confirm_cnt, nav_a, nav_b);
}

void dialogue_init_assign_scope(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const int is_dir, const int is_app,
    const int at_root, const char *nav_a, const char *nav_b
) {
    const char *opts[4];
    int n = 0;

    if (!is_dir) {
        dlg->option_data[n] = casn_single;
        opts[n] = lang.generic.content;
        n++;
    }

    if (!is_app) {
        dlg->option_data[n] = casn_dir;
        opts[n] = lang.generic.directory;
        n++;

        if (!at_root) {
            dlg->option_data[n] = casn_parent;
            opts[n] = lang.generic.recursive;
            n++;
        }
    }

    dlg->option_data[n] = -1;
    opts[n] = lang.generic.discard;
    n++;

    dialogue_init(dlg, t, parent, title, lang.generic.assign_desc, opts, n, nav_a, nav_b);
}

void dialogue_init_warn(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *description, const char *nav_a,
    const char *nav_b
) {
    const char *opts[] = {lang.generic.understand, lang.generic.cancel};
    dialogue_init(dlg, t, parent, lang.generic.warning, description, opts, 2, nav_a, nav_b);
}

void dialogue_init_remove(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *description, const char *nav_a,
    const char *nav_b
) {
    const char *opts[mux_remove_cnt] = {lang.generic.remove, lang.generic.skip_confirm, lang.generic.cancel};
    dialogue_init(dlg, t, parent, lang.generic.confirm, description, opts, mux_remove_cnt, nav_a, nav_b);
}

void dialogue_init_message(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *message, const char *nav_b
) {
    dlg->option_count = 0;
    dlg->selected = 0;
    dlg->theme = t;

    dlg->dim = lv_obj_create(parent);
    lv_obj_set_size(dlg->dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dlg->dim, 0, 0);
    lv_obj_set_style_bg_color(dlg->dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->dim, t->dialogue.dim_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    dlg->dim_alpha = t->dialogue.dim_alpha;

    dlg->panel = lv_obj_create(parent);
    lv_obj_set_size(dlg->panel, lv_pct(60), LV_SIZE_CONTENT);
    lv_obj_align(dlg->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dlg->panel, lv_color_hex(t->dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->panel, t->dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(dlg->panel, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(dlg->panel, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->panel, t->dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(dlg->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(dlg->panel, LV_FLEX_ALIGN_START, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_shadow_zone_register(
        dlg->panel, lv_color_hex(t->dialogue.shadow_colour), (lv_opa_t) t->dialogue.shadow_alpha,
        (int8_t) t->dialogue.shadow_x_offset, (int8_t) t->dialogue.shadow_y_offset,
        lv_color_hex(t->dialogue.shadow_colour_focus), (lv_opa_t) t->dialogue.shadow_alpha_focus,
        (int8_t) t->dialogue.shadow_x_offset_focus, (int8_t) t->dialogue.shadow_y_offset_focus
    );

    dlg->title_label = lv_label_create(dlg->panel);
    lv_label_set_text(dlg->title_label, title);
    lv_obj_set_width(dlg->title_label, LV_PCT(100));
    lv_obj_set_style_text_align(dlg->title_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->title_label, lv_color_hex(t->dialogue.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->title_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->title_label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);

    dlg->description_label = NULL;
    if (description) {
        dlg->description_label = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->description_label, description);
        lv_obj_set_width(dlg->description_label, LV_PCT(100));
        lv_obj_set_style_text_align(dlg->description_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->description_label, lv_color_hex(t->dialogue.content), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->description_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(dlg->description_label, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(dlg->description_label, 8, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_long_mode(dlg->description_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *sep_top = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep_top, LV_PCT(100), 1);
    lv_obj_clear_flag(sep_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep_top, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_top, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
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
    lv_obj_set_width(dlg->options[0], LV_PCT(100));
    lv_obj_set_style_text_align(dlg->options[0], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->options[0], lv_color_hex(t->dialogue.content), MU_OBJ_MAIN_DEFAULT);
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
    lv_obj_set_style_bg_color(sep_bot, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep_bot, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
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

    const lv_obj_t *glyph_b = create_footer_glyph(footer_row, t, "b", t->nav.b, 0);
    lv_obj_t *label_b = create_footer_text(footer_row, t, t->nav.b.text, t->nav.b.text_alpha, 0);
    lv_label_set_text(label_b, nav_b);

    (void) glyph_b;
}

void dialogue_init_accept(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const char *title, const char *description,
    const char *nav_a
) {
    dlg->option_count = 0;
    dlg->selected = 0;
    dlg->theme = t;

    dlg->dim = lv_obj_create(parent);
    lv_obj_set_size(dlg->dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(dlg->dim, 0, 0);
    lv_obj_set_style_bg_color(dlg->dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->dim, t->dialogue.dim_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->dim, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    dlg->dim_alpha = t->dialogue.dim_alpha;

    dlg->panel = lv_obj_create(parent);
    lv_obj_set_size(dlg->panel, lv_pct(60), LV_SIZE_CONTENT);
    lv_obj_align(dlg->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dlg->panel, lv_color_hex(t->dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->panel, t->dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(dlg->panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(dlg->panel, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(dlg->panel, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(dlg->panel, t->dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_left(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(dlg->panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(dlg->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(dlg->panel, LV_FLEX_ALIGN_START, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_shadow_zone_register(
        dlg->panel, lv_color_hex(t->dialogue.shadow_colour), (lv_opa_t) t->dialogue.shadow_alpha,
        (int8_t) t->dialogue.shadow_x_offset, (int8_t) t->dialogue.shadow_y_offset,
        lv_color_hex(t->dialogue.shadow_colour_focus), (lv_opa_t) t->dialogue.shadow_alpha_focus,
        (int8_t) t->dialogue.shadow_x_offset_focus, (int8_t) t->dialogue.shadow_y_offset_focus
    );

    dlg->title_label = lv_label_create(dlg->panel);
    lv_label_set_text(dlg->title_label, title);
    lv_obj_set_width(dlg->title_label, LV_PCT(100));
    lv_obj_set_style_text_align(dlg->title_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(dlg->title_label, lv_color_hex(t->dialogue.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dlg->title_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(dlg->title_label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(dlg->title_label, 8, MU_OBJ_MAIN_DEFAULT);

    dlg->description_label = NULL;
    if (description) {
        dlg->description_label = lv_label_create(dlg->panel);
        lv_label_set_text(dlg->description_label, description);
        lv_obj_set_width(dlg->description_label, LV_PCT(100));
        lv_obj_set_style_text_align(dlg->description_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(dlg->description_label, lv_color_hex(t->dialogue.content), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(dlg->description_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(dlg->description_label, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(dlg->description_label, 8, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_long_mode(dlg->description_label, LV_LABEL_LONG_WRAP);
    }

    lv_obj_t *sep = lv_obj_create(dlg->panel);
    lv_obj_set_size(sep, LV_PCT(100), 1);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sep, lv_color_hex(t->dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(sep, t->dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(sep, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(sep, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(sep, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *gap = lv_obj_create(dlg->panel);
    lv_obj_set_size(gap, LV_PCT(100), 8);
    lv_obj_clear_flag(gap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(gap, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(gap, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(gap, 0, MU_OBJ_MAIN_DEFAULT);

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

    const lv_obj_t *glyph_a = create_footer_glyph(footer_row, t, "a", t->nav.a, 0);
    lv_obj_t *label_a = create_footer_text(footer_row, t, t->nav.a.text, t->nav.a.text_alpha, 0);
    lv_label_set_text(label_a, nav_a);
    (void) glyph_a;
}

void dialogue_show(mux_dialogue *dlg) {
    lv_anim_del(dlg->panel, panel_anim_y_cb);
    lv_anim_del(dlg->panel, panel_anim_x_cb);
    lv_anim_del(dlg->panel, panel_anim_opa_cb);
    lv_anim_del(dlg->dim, dim_anim_opa_cb);
    lv_obj_set_style_opa(dlg->panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);

    lv_obj_move_foreground(dlg->dim);
    lv_obj_move_foreground(dlg->panel);

    lv_obj_set_style_bg_opa(dlg->dim, dlg->dim_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);

    if (dlg->theme) dialogue_refresh(dlg, dlg->theme);

    lv_coord_t panel_h = 0;
    static const uint8_t dlg_width_pct[] = {60, 75, 90};
    const lv_coord_t dlg_max_h = LV_VER_RES * 92 / 100;
    for (size_t i = 0; i < sizeof(dlg_width_pct) / sizeof(dlg_width_pct[0]); i++) {
        lv_obj_set_width(dlg->panel, lv_pct(dlg_width_pct[i]));
        lv_obj_update_layout(dlg->panel);
        panel_h = lv_obj_get_height(dlg->panel);
        if (panel_h <= dlg_max_h) break;
    }

    const lv_coord_t target_y = (LV_VER_RES - panel_h) / 2;
    lv_obj_set_style_align(dlg->panel, LV_ALIGN_TOP_MID, 0);

    page_nav_blocked = 1;
    timer_suspend_all();

    const int diag_transition = config.visual.dialogue_transition;

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
            lv_anim_set_exec_cb(&ap, panel_anim_opa_cb);
            lv_anim_set_values(&ap, LV_OPA_TRANSP, LV_OPA_COVER);
            break;
        case TSN_SLIDE_RIGHT:
        case TSN_BOUNCE_RIGHT:
        case TSN_SHOOT_RIGHT:
            lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_y(dlg->panel, target_y);
            lv_obj_set_style_translate_x(dlg->panel, LV_HOR_RES, MU_OBJ_MAIN_DEFAULT);
            lv_anim_set_exec_cb(&ap, panel_anim_x_cb);
            lv_anim_set_values(&ap, LV_HOR_RES, 0);
            break;
        case TSN_SLIDE_LEFT:
        case TSN_BOUNCE_LEFT:
        case TSN_SHOOT_LEFT:
            lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_y(dlg->panel, target_y);
            lv_obj_set_style_translate_x(dlg->panel, -LV_HOR_RES, MU_OBJ_MAIN_DEFAULT);
            lv_anim_set_exec_cb(&ap, panel_anim_x_cb);
            lv_anim_set_values(&ap, -LV_HOR_RES, 0);
            break;
        case TSN_SLIDE_DOWN:
        case TSN_BOUNCE_DOWN:
        case TSN_SHOOT_DOWN:
            lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_y(dlg->panel, -panel_h);
            lv_anim_set_exec_cb(&ap, panel_anim_y_cb);
            lv_anim_set_values(&ap, -panel_h, target_y);
            break;
        case TSN_DISABLED:
            lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_y(dlg->panel, target_y);
            dialogue_force_redraw(dlg);
            goto skip_panel_anim;
        default:
            lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_y(dlg->panel, LV_VER_RES);
            lv_anim_set_exec_cb(&ap, panel_anim_y_cb);
            lv_anim_set_values(&ap, LV_VER_RES, target_y);
            break;
    }

    lv_anim_set_user_data(&ap, dlg);
    lv_anim_set_ready_cb(&ap, show_anim_ready_cb);
    lv_anim_set_time(&ap, duration);
    lv_anim_set_path_cb(&ap, path);

    lv_anim_start(&ap);
skip_panel_anim:;
}

void dialogue_hide(const mux_dialogue *dlg) {
    page_nav_blocked = 0;
    lv_anim_del(dlg->panel, panel_anim_y_cb);
    lv_anim_del(dlg->panel, panel_anim_x_cb);
    lv_anim_del(dlg->panel, panel_anim_opa_cb);
    lv_anim_del(dlg->dim, dim_anim_opa_cb);

    lv_obj_add_flag(dlg->dim, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(dlg->panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_set_style_opa(dlg->panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(dlg->panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_refr_now(NULL);
    timer_resume_all();
}

void dialogue_navigate(mux_dialogue *dlg, struct theme_config *t, const int delta) {
    if (dlg->option_count <= 0) return;

    dlg->selected = (dlg->selected + delta + dlg->option_count) % dlg->option_count;
    dialogue_refresh(dlg, t);
}

void dialogue_refresh(const mux_dialogue *dlg, const struct theme_config *t) {
    for (int i = 0; i < dlg->option_count; i++) {
        const int sel = i == dlg->selected;
        lv_obj_set_style_text_color(
            dlg->options[i], lv_color_hex(sel ? t->dialogue.option : t->dialogue.content), MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_bg_color(dlg->options[i], lv_color_hex(t->dialogue.selection), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(
            dlg->options[i], sel ? t->dialogue.selection_alpha : LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT
        );
        if (sel) {
            lv_obj_add_state(dlg->options[i], LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(dlg->options[i], LV_STATE_CHECKED);
        }
    }
}

void dialogue_open(int *active, mux_dialogue *dlg, struct theme_config *t) {
    *active = 1;
    dlg->selected = 0;
    dialogue_show(dlg);
    dialogue_refresh(dlg, t);
}

void dialogue_dismiss(int *active, const mux_dialogue *dlg) {
    *active = 0;
    dialogue_hide(dlg);
}

void dialogue_handle_dpad(mux_dialogue *dlg, struct theme_config *t, const int direction, const int should_fire) {
    if (!should_fire) return;

    dialogue_navigate(dlg, t, direction);
    play_sound(snd_navigate);
}

int dialogue_guard_unsaved(int *active, mux_dialogue *dlg, struct theme_config *t, const int is_modified) {
    if (config.settings.advanced.trust_modify || !is_modified) return 0;

    dialogue_open(active, dlg, t);
    return 1;
}
