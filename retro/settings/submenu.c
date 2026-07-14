#include <stdio.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "settings.h"
#include "submenu.h"

#define SUBMENU_VALUE_MAX 64

static int row_is_action(const submenu *m, const int index) {
    return m->def->row_is_action ? m->def->row_is_action(index) : 0;
}

static void row_value(const submenu *m, const int index, char *buf, const size_t len) {
    buf[0] = '\0';
    if (m->def->value_text) m->def->value_text(index, buf, len);
}

static void build_row(const submenu *m, const int index) {
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = lv_label_create(panel);

    char value_text[SUBMENU_VALUE_MAX];
    row_value(m, index, value_text, sizeof(value_text));

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, m->def->labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", m->def->glyphs[index]);
    apply_theme_list_value(&theme, value, value_text);
    apply_size_to_content(&theme, ui_pnl_content, label, icon, m->def->labels[index]);
    apply_text_long_dot(&theme, label);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    lv_group_add_obj(ui_group_value, value);
}

static void rebuild_rows(const submenu *m) {
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;

    for (int i = 0; i < m->def->row_count; i++)
        build_row(m, i);

    ui_count_static = m->def->row_count;
    first_open = 0;
}

static void focus_row(const int index) {
    if (index < 0 || index >= ui_count_static) return;
    current_item_index = index;

    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *label = lv_obj_get_child(panel, 0);
    lv_obj_t *glyph = lv_obj_get_child(panel, 1);
    lv_obj_t *value = lv_obj_get_child(panel, 2);

    nav_suppress_next_shake();

    if (label) lv_group_focus_obj(label);
    if (glyph) lv_group_focus_obj(glyph);
    if (value) lv_group_focus_obj(value);
    lv_group_focus_obj(panel);

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
}

static void refresh_row(const submenu *m, const int index, const enum nav_direction shake_dir) {
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    char value_text[SUBMENU_VALUE_MAX];
    row_value(m, index, value_text, sizeof(value_text));
    lv_label_set_text(value, value_text);
    nav_play_shake(value, shake_dir);
}

void submenu_refresh_values(const submenu *m) {
    for (int i = 0; i < m->def->row_count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, i);
        if (!panel) continue;

        lv_obj_t *value = lv_obj_get_child(panel, 2);
        if (!value) continue;

        char value_text[SUBMENU_VALUE_MAX];
        row_value(m, i, value_text, sizeof(value_text));
        lv_label_set_text(value, value_text);
    }
}

static void submenu_nav(submenu *m, const int force) {
    const int action_row = row_is_action(m, current_item_index);
    if (!force && action_row == m->nav_row_class) return;
    m->nav_row_class = action_row;

    if (action_row) {
        nav_show_lr(0);
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.select, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    } else {
        nav_show_a(0, "");
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.generic.change, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    }
    pause_menu_fix_nav_order();
}

static void close_menu(submenu *m) {
    m->active = 0;
    if (m->def->closed) m->def->closed();
}

void submenu_init(submenu *m, const submenu_def *def) {
    static const char *save_options[4];
    save_options[0] = lang.muxretro.save.content_save;
    save_options[1] = lang.muxretro.save.core_save;
    save_options[2] = lang.muxretro.save.directory_save;
    save_options[3] = lang.generic.discard;

    m->def = def;
    m->active = 0;
    m->save_dialogue_active = 0;
    m->nav_row_class = -1;

    dialogue_init(
        &m->save_dlg, &theme, ui_screen, def->save_title, def->save_desc, save_options, 4, lang.generic.select,
        lang.generic.cancel
    );
}

void submenu_open(submenu *m) {
    m->active = 1;
    m->prev_nav_mask = nav_mask_standard();
    m->nav_row_class = -1;

    rebuild_rows(m);
    focus_row(current_item_index);
    submenu_nav(m, 1);
}

void submenu_reopen_at(submenu *m, const int row) {
    rebuild_rows(m);
    focus_row(row);
    m->nav_row_class = -1;
    submenu_nav(m, 1);
    m->prev_nav_mask = nav_mask_standard();
    pause_menu_sync_input_mask();
}

int submenu_is_active(const submenu *m) {
    return m->active;
}

void submenu_tick(submenu *m) {
    if (m->def->child_tick && m->def->child_tick()) return;

    const uint64_t mask = nav_mask_standard();
    const uint64_t edge = mask & ~m->prev_nav_mask;
    m->prev_nav_mask = mask;

    if (nav_input_halted()) return;

    if (m->save_dialogue_active) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&m->save_dlg, &theme, (edge & BIT(1)) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = m->save_dlg.selected;
            dialogue_dismiss(&m->save_dialogue_active, &m->save_dlg);
            play_sound(snd_confirm);

            session_settings_apply_save_choice(opt);
            close_menu(m);
        } else if (edge & BIT(5)) {
            dialogue_dismiss(&m->save_dialogue_active, &m->save_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&m->rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&m->rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    const int cycle_allowed = m->def->cycle != NULL && !row_is_action(m, current_item_index);
    const int do_left = nav_repeat_step(&m->rpt_left, edge & BIT(2), mask & BIT(2), cycle_allowed, now);
    const int do_right = nav_repeat_step(&m->rpt_right, edge & BIT(3), mask & BIT(3), cycle_allowed, now);

    if (do_up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 2, 0, 1);
        submenu_nav(m, 0);
    } else if (do_down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 2, 0, 1);
        submenu_nav(m, 0);
    } else if (do_left && cycle_allowed) {
        m->def->cycle(current_item_index, -1);
        refresh_row(m, current_item_index, nav_dir_left);
        play_sound(snd_option);
    } else if (do_right && cycle_allowed) {
        m->def->cycle(current_item_index, +1);
        refresh_row(m, current_item_index, nav_dir_right);
        play_sound(snd_option);
    } else if (nav_page_tick(edge, mask, 2)) {
        submenu_nav(m, 0);
    } else if (edge & BIT(4)) {
        if (row_is_action(m, current_item_index) && m->def->action) {
            play_sound(snd_confirm);
            m->def->action(current_item_index);
        }
    } else if (edge & BIT(5)) {
        if (session_settings_is_dirty()) {
            play_sound(snd_confirm);
            dialogue_open(&m->save_dialogue_active, &m->save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_menu(m);
        }
    }
}
