#include <stdio.h>
#include <string.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/ui/common.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "settings.h"
#include "submenu.h"

#define SUBMENU_VALUE_MAX       64
#define SUBMENU_STACK_MAX       8
#define SUBMENU_HELP_MAX        64
#define SUBMENU_SECTION_ROW_MAX 96

static submenu *submenu_stack[SUBMENU_STACK_MAX];
static int submenu_stack_depth = 0;
static lv_obj_t *section_panels[SUBMENU_SECTION_ROW_MAX];
static lv_obj_t *section_labels[SUBMENU_SECTION_ROW_MAX];
static lv_obj_t *section_glyphs[SUBMENU_SECTION_ROW_MAX];
static lv_obj_t *section_values[SUBMENU_SECTION_ROW_MAX];

#define SUBMENU_NAV_X_BIT  BIT(6)
#define SUBMENU_NAV_Y_BIT  BIT(7)
#define SUBMENU_NAV_L2_BIT BIT(14)
#define SUBMENU_NAV_R2_BIT BIT(15)

static int row_is_action(const submenu *m, const int index) {
    return m->def->row_is_action ? m->def->row_is_action(index) : 0;
}

static int sectioned(const submenu *m) {
    return m->def->frames && m->def->frame_count > 0;
}

static int selected_row(const submenu *m) {
    return sectioned(m) ? list_frame_current_row() : current_item_index;
}

static int row_can_cycle(const submenu *m, const int index) {
    if (!m->def->cycle) return 0;
    return m->def->row_can_cycle ? m->def->row_can_cycle(index) : !row_is_action(m, index);
}

static const char *row_extra_label(const submenu *m, const int index) {
    return m->def->extra_label && m->def->extra_action ? m->def->extra_label(index) : NULL;
}

static const char *row_action_label(const submenu *m, const int index) {
    const char *label = m->def->action_label ? m->def->action_label(index) : NULL;
    return label ? label : lang.generic.select;
}

static const char *row_y_label(const submenu *m, const int index) {
    return m->def->y_label && m->def->y_action ? m->def->y_label(index) : NULL;
}

static uint64_t submenu_nav_mask(void) {
    return nav_mask_standard() | (mux_input_pressed(mux_input_x) ? SUBMENU_NAV_X_BIT : 0)
           | (mux_input_pressed(mux_input_y) ? SUBMENU_NAV_Y_BIT : 0)
           | (mux_input_pressed(mux_input_l2) ? SUBMENU_NAV_L2_BIT : 0)
           | (mux_input_pressed(mux_input_r2) ? SUBMENU_NAV_R2_BIT : 0);
}

static void nav_show_y(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_y, text);
        lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_x(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_x, text);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void row_value(const submenu *m, const int index, char *buf) {
    buf[0] = '\0';
    if (m->def->value_text) m->def->value_text(index, buf, SUBMENU_VALUE_MAX);
}

static void build_row(const submenu *m, const int index) {
    const int create_value_label = !m->def->skip_value_object_creation;
    lv_obj_t *panel = lv_obj_create(ui_pnl_content);
    lv_obj_t *label = lv_label_create(panel);
    lv_obj_t *icon = lv_img_create(panel);
    lv_obj_t *value = create_value_label ? lv_label_create(panel) : NULL;

    apply_theme_list_panel(panel);
    apply_theme_option_item_label(&theme, label, m->def->labels[index], 1);
    apply_theme_list_glyph(&theme, icon, "muxretro", m->def->glyphs[index]);
    if (value) {
        char value_text[SUBMENU_VALUE_MAX];
        row_value(m, index, value_text);
        apply_theme_list_value(&theme, value, value_text);
    }

    apply_size_to_content(&theme, ui_pnl_content, label, icon, m->def->labels[index]);
    apply_text_long_dot(&theme, label);

    if (m->def->help) lv_obj_set_user_data(label, (void *) m->def->help[index]);

    lv_group_add_obj(ui_group, label);
    lv_group_add_obj(ui_group_glyph, icon);
    lv_group_add_obj(ui_group_panel, panel);
    if (value) lv_group_add_obj(ui_group_value, value);

    if (sectioned(m) && index < SUBMENU_SECTION_ROW_MAX) {
        section_panels[index] = panel;
        section_labels[index] = label;
        section_glyphs[index] = icon;
        section_values[index] = value;
    }
}

static void rebuild_rows(const submenu *m) {
    list_frame_reset();
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    memset(section_panels, 0, sizeof(section_panels));
    memset(section_labels, 0, sizeof(section_labels));
    memset(section_glyphs, 0, sizeof(section_glyphs));
    memset(section_values, 0, sizeof(section_values));

    ui_count_static = 0;
    current_item_index = 0;

    for (int i = 0; i < m->def->row_count; i++)
        build_row(m, i);

    if (sectioned(m)
        && list_frame_init(
            &theme, ui_pnl_content, m->def->frames, m->def->frame_count, section_panels, section_labels, section_glyphs,
            section_values, m->def->row_count
        )) {
        list_frame_apply();
        first_open = 0;
        return;
    }

    ui_count_static = m->def->row_count;
    first_open = 0;
}

static void focus_row(const submenu *m, const int index) {
    if (index < 0 || index >= m->def->row_count) return;

    lv_obj_t *panel;
    if (sectioned(m)) {
        for (int frame = 0; frame < m->def->frame_count; frame++) {
            const list_frame *section = &m->def->frames[frame];
            if (index < section->first || index >= section->first + section->count) continue;
            if (list_frame_current() != frame) list_frame_go(frame);
            current_item_index = index - section->first + 1;
            break;
        }
        panel = section_panels[index];
    } else {
        if (index >= ui_count_static) return;
        current_item_index = index;
        panel = lv_obj_get_child(ui_pnl_content, index);
    }
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
    lv_obj_t *panel = sectioned(m) ? section_panels[index] : lv_obj_get_child(ui_pnl_content, index);
    if (!panel) return;

    lv_obj_t *value = lv_obj_get_child(panel, 2);
    if (!value) return;

    char value_text[SUBMENU_VALUE_MAX];
    row_value(m, index, value_text);
    if (strcmp(lv_label_get_text(value), value_text) != 0) lv_label_set_text(value, value_text);
    nav_play_shake(value, shake_dir);
}

void submenu_refresh_values(const submenu *m) {
    for (int i = 0; i < m->def->row_count; i++) {
        lv_obj_t *panel = sectioned(m) ? section_panels[i] : lv_obj_get_child(ui_pnl_content, i);
        if (!panel) continue;

        lv_obj_t *value = lv_obj_get_child(panel, 2);
        if (!value) continue;

        char value_text[SUBMENU_VALUE_MAX];
        row_value(m, i, value_text);
        if (strcmp(lv_label_get_text(value), value_text) != 0) lv_label_set_text(value, value_text);
    }
}

static void submenu_show_help(const submenu *m) {
    if (!m->def->help) return;

    if (sectioned(m)) {
        const int row = selected_row(m);
        if (row < 0) {
            list_frame_help();
        } else {
            show_info_box(m->def->labels[row], m->def->help[row], 0);
        }
        return;
    }

    struct help_msg help_messages[SUBMENU_HELP_MAX];

    int count = m->def->row_count;
    if (count > SUBMENU_HELP_MAX) count = SUBMENU_HELP_MAX;

    for (int i = 0; i < count; i++) {
        help_messages[i].key = (char *) m->def->help[i];
        help_messages[i].message = (char *) m->def->help[i];
    }

    gen_help(current_item_index, help_messages, (size_t) count, ui_group, NULL);
}

static void submenu_nav(submenu *m, const int force) {
    const int row = selected_row(m);
    if (row < 0) {
        nav_show_x(0, NULL);
        nav_show_y(0, NULL);
        nav_show_a(0, NULL);
        nav_show_lr(1);
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.generic.change, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
        pause_menu_fix_nav_order();
        return;
    }

    const int action_row = row_is_action(m, row);
    const int cycle_row = row_can_cycle(m, row);
    const char *action = row_action_label(m, row);
    const char *extra = row_extra_label(m, row);
    const char *y = row_y_label(m, row);
    const int row_class = action_row | (cycle_row << 1);
    if (!force && row_class == m->nav_row_class && action == m->nav_action_label && extra == m->nav_extra_label
        && y == m->nav_y_label)
        return;
    m->nav_row_class = row_class;
    m->nav_action_label = action;
    m->nav_extra_label = extra;
    m->nav_y_label = y;

    nav_show_x(extra != NULL, extra);
    nav_show_y(y != NULL, y);

    if (action_row) {
        nav_show_lr(cycle_row);
        if (cycle_row)
            setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                          {ui_lbl_nav_lr, lang.generic.change, 0},
                                          {ui_lbl_nav_a_glyph, "", 0},
                                          {ui_lbl_nav_a, action, 0},
                                          {ui_lbl_nav_b_glyph, "", 0},
                                          {ui_lbl_nav_b, lang.generic.back, 0},
                                          {NULL, NULL, 0}});
        else
            setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                          {ui_lbl_nav_a, action, 0},
                                          {ui_lbl_nav_b_glyph, "", 0},
                                          {ui_lbl_nav_b, lang.generic.back, 0},
                                          {NULL, NULL, 0}});
    } else if (cycle_row) {
        nav_show_a(0, "");
        nav_show_lr(1);
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.generic.change, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
    } else {
        nav_show_a(0, "");
        nav_show_lr(0);
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}}
        );
    }
    pause_menu_fix_nav_order();
}

void submenu_refresh_nav(submenu *m) {
    if (!m || !m->active) return;
    submenu_nav(m, 0);
}

static void close_menu(submenu *m) {
    m->active = 0;
    nav_show_x(0, NULL);
    nav_show_y(0, NULL);
    if (sectioned(m)) {
        if (m->def->remember_section_key) list_frame_remember_section_key(m->def->remember_section_key);
        list_frame_reset();
    }
    if (submenu_stack_depth > 0 && submenu_stack[submenu_stack_depth - 1] == m) submenu_stack_depth--;
    if (m->def->closed) m->def->closed();
}

void submenu_stack_resync(void) {
    for (int i = 0; i < submenu_stack_depth; i++)
        submenu_stack[i]->entry_snapshot = session_settings;
}

void submenu_init(submenu *m, const submenu_def *def) {
    static const char *save_options[5];
    save_options[0] = lang.muxretro.save.content_save;
    save_options[1] = lang.muxretro.save.core_save;
    save_options[2] = lang.muxretro.save.directory_save;
    save_options[3] = lang.muxretro.save.session_save;
    save_options[4] = lang.generic.discard;

    m->def = def;
    m->active = 0;
    m->save_dlg.active = 0;
    m->save_all_dlg.active = 0;
    m->nav_row_class = -1;
    m->nav_action_label = NULL;
    m->nav_extra_label = NULL;
    m->nav_y_label = NULL;
    m->pending_action_row = -1;

    dialogue_init(
        &m->save_dlg, &theme, ui_screen, def->save_title, def->save_desc, save_options, 5, lang.generic.select,
        lang.generic.cancel
    );

    if (def->row_is_save)
        dialogue_init(
            &m->save_all_dlg, &theme, ui_screen, lang.muxretro.save_settings, def->save_desc, save_options, 3,
            lang.generic.select, lang.generic.cancel
        );
}

void submenu_open(submenu *m) {
    m->active = 1;
    m->prev_nav_mask = submenu_nav_mask();
    m->nav_row_class = -1;
    m->nav_action_label = NULL;
    m->nav_extra_label = NULL;
    m->nav_y_label = NULL;
    m->entry_snapshot = session_settings;

    if (submenu_stack_depth < SUBMENU_STACK_MAX) submenu_stack[submenu_stack_depth++] = m;

    rebuild_rows(m);
    if (sectioned(m)) {
        const int steps = m->def->remember_section_key ? list_frame_restore_key(m->def->remember_section_key) : 0;
        gen_step_movement(steps, +1, 2, 0, 0);
    } else {
        focus_row(m, current_item_index);
    }
    submenu_nav(m, 1);
}

void submenu_reopen_at(submenu *m, const int row) {
    rebuild_rows(m);
    focus_row(m, row);
    m->nav_row_class = -1;
    m->nav_action_label = NULL;
    m->nav_extra_label = NULL;
    m->nav_y_label = NULL;
    submenu_nav(m, 1);
    m->prev_nav_mask = submenu_nav_mask();
    pause_menu_sync_input_mask();
}

void submenu_focus_at(submenu *m, const int row) {
    if (!m || !m->active) return;
    focus_row(m, row);
}

int submenu_is_active(const submenu *m) {
    return m->active;
}

static int coarse_step_tick(submenu *m, const uint64_t edge, const uint64_t mask) {
    if (dialogue_active(&m->save_dlg) || dialogue_active(&m->save_all_dlg)) return 0;

    const int row = selected_row(m);
    const int step = row >= 0 && m->def->row_coarse_step ? m->def->row_coarse_step(row) : 0;
    if (step <= 0 || !row_can_cycle(m, row)) return 0;

    const uint32_t now = SDL_GetTicks();
    const int down =
        nav_repeat_step(&m->rpt_l2, (edge & SUBMENU_NAV_L2_BIT) != 0, (mask & SUBMENU_NAV_L2_BIT) != 0, 1, now);
    const int up =
        nav_repeat_step(&m->rpt_r2, (edge & SUBMENU_NAV_R2_BIT) != 0, (mask & SUBMENU_NAV_R2_BIT) != 0, 1, now);

    if (!down && !up) return 0;

    m->def->cycle(row, up ? step : -step);
    refresh_row(m, row, up ? nav_dir_right : nav_dir_left);
    play_sound(snd_option);

    return 1;
}

void submenu_tick(submenu *m) {
    if (m->def->child_tick && m->def->child_tick()) return;

    const uint64_t mask = submenu_nav_mask();
    const uint64_t edge = mask & ~m->prev_nav_mask;
    m->prev_nav_mask = mask;

    if (!dialogue_active(&m->save_dlg) && !dialogue_active(&m->save_all_dlg)) {
        const int menu_tap = pause_menu_take_menu_tap();
        if (pause_menu_help_input(edge & BIT(0), edge & BIT(1), menu_tap || edge & (BIT(4) | BIT(5)))) return;

        if (menu_tap) {
            submenu_show_help(m);
            return;
        }
    }

    if (coarse_step_tick(m, edge, mask)) return;
    if (nav_input_halted()) return;

    if (dialogue_active(&m->save_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&m->save_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = m->save_dlg.selected;
            dialogue_dismiss(&m->save_dlg);

            if (opt == 4) {
                session_settings_discard_to(&m->entry_snapshot);
            } else if (opt == 3) {
                submenu_stack_resync();
            } else {
                session_settings_apply_save_choice(opt);
                submenu_stack_resync();
            }

            if (m->pending_action_row >= 0) {
                const int row = m->pending_action_row;
                m->pending_action_row = -1;
                if (m->def->action) m->def->action(row);
            } else {
                close_menu(m);
            }
        } else if (edge & BIT(5)) {
            m->pending_action_row = -1;
            dialogue_mark_cancelled(&m->save_dlg);
            dialogue_dismiss(&m->save_dlg);
        }
        return;
    }

    if (dialogue_active(&m->save_all_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&m->save_all_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int opt = m->save_all_dlg.selected;
            dialogue_dismiss(&m->save_all_dlg);

            session_settings_apply_save_choice(opt);
            submenu_stack_resync();
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&m->save_all_dlg);
            dialogue_dismiss(&m->save_all_dlg);
        }
        return;
    }

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(&m->rpt_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int do_down =
        nav_repeat_step(&m->rpt_down, edge & BIT(1), mask & BIT(1), current_item_index < ui_count_static - 1, now);
    const int row = selected_row(m);
    const int cycle_allowed = row >= 0 && row_can_cycle(m, row);
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
    } else if (edge & BIT(2) && sectioned(m) && list_frame_focused()) {
        if (list_frame_move(-1)) {
            gen_step_movement(0, +1, 2, 0, 0);
            play_sound(snd_option);
            submenu_nav(m, 1);
        }
    } else if (edge & BIT(3) && sectioned(m) && list_frame_focused()) {
        if (list_frame_move(+1)) {
            gen_step_movement(0, +1, 2, 0, 0);
            play_sound(snd_option);
            submenu_nav(m, 1);
        }
    } else if (do_left && cycle_allowed) {
        m->def->cycle(row, -1);
        refresh_row(m, row, nav_dir_left);
        play_sound(snd_option);
    } else if (do_right && cycle_allowed) {
        m->def->cycle(row, +1);
        refresh_row(m, row, nav_dir_right);
        play_sound(snd_option);
    } else if (sectioned(m) && mask & (NAV_PAGE_UP_BIT | NAV_PAGE_DOWN_BIT)) {
        const int direction = edge & NAV_PAGE_UP_BIT ? -1 : edge & NAV_PAGE_DOWN_BIT ? +1 : 0;
        if (direction && list_frame_move(direction)) {
            gen_step_movement(0, +1, 2, 0, 0);
            play_sound(snd_option);
            submenu_nav(m, 1);
        }
    } else if (nav_page_tick(edge, mask, 2)) {
        submenu_nav(m, 0);
    } else if (edge & BIT(4)) {
        if (row >= 0 && m->def->row_is_save && m->def->row_is_save(row)) {
            play_sound(snd_confirm);
            dialogue_open(&m->save_all_dlg, &theme);
        } else if (row >= 0 && row_is_action(m, row) && m->def->action) {
            if (m->def->child_tick && !m->def->action_without_save_guard
                && memcmp(&session_settings, &m->entry_snapshot, sizeof(session_settings)) != 0) {
                play_sound(snd_confirm);
                m->pending_action_row = row;
                dialogue_open(&m->save_dlg, &theme);
            } else {
                play_sound(snd_confirm);
                m->def->action(row);
            }
        }
    } else if (edge & SUBMENU_NAV_X_BIT) {
        if (row >= 0 && row_extra_label(m, row)) {
            play_sound(snd_confirm);
            m->def->extra_action(row);
        }
    } else if (edge & SUBMENU_NAV_Y_BIT) {
        if (row >= 0 && row_y_label(m, row)) {
            play_sound(snd_confirm);
            m->def->y_action(row);
        }
    } else if (edge & BIT(5)) {
        if (memcmp(&session_settings, &m->entry_snapshot, sizeof(session_settings)) != 0) {
            play_sound(snd_confirm);
            dialogue_open(&m->save_dlg, &theme);
        } else {
            play_sound(snd_back);
            close_menu(m);
        }
    }
}
