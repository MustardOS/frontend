#include <stdio.h>
#include <common/platform/input.h>
#include <common/ui/common.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_button_assignments = 0,
    row_ff_enabled,
    row_ff_speed,
    row_ff_audio_enabled,
    row_ff_glyph_enabled,
    row_slowmo_enabled,
    row_slowmo_speed,
    row_slowmo_audio_enabled,
    row_slowmo_glyph_enabled,
    row_speed_persistence,
    row_pause_enabled,
    row_pause_glyph_enabled,
    row_quicksave_enabled,
    row_quickload_enabled,
    row_toggle_fps_enabled,
    row_header_toggle_enabled,
    row_quit_enabled,
    row_manual_enabled,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.hotkeys_screen.button_assignments,
    lang.muxretro.hotkeys_screen.fast_forward,
    lang.muxretro.hotkeys_screen.ff_speed,
    lang.muxretro.hotkeys_screen.ff_audio,
    lang.muxretro.hotkeys_screen.ff_glyph,
    lang.muxretro.hotkeys_screen.slow_motion,
    lang.muxretro.hotkeys_screen.slowmo_speed,
    lang.muxretro.hotkeys_screen.slowmo_audio,
    lang.muxretro.hotkeys_screen.slowmo_glyph,
    lang.muxretro.hotkeys_screen.speed_persistence,
    lang.muxretro.hotkeys_screen.pause_content,
    lang.muxretro.hotkeys_screen.pause_glyph,
    lang.muxretro.hotkeys_screen.quick_save,
    lang.muxretro.hotkeys_screen.quick_load,
    lang.muxretro.hotkeys_screen.toggle_fps,
    lang.muxretro.hotkeys_screen.toggle_header,
    lang.muxretro.quit,
    lang.muxretro.hotkeys_screen.manual
};

static const char *row_glyphs[row_count] = {"hotkeys",      "fastforward", "ffspeed",   "sound",       "ffglyph",
                                            "slowmotion",   "slowmospeed", "sound",     "slowmoglyph", "fastforward",
                                            "pause",        "pause",       "quicksave", "quickload",   "togglefps",
                                            "toggleheader", "quit",        "manual"};

static const char *row_help[row_count] = {
    lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.fast_forward,
    lang.muxretro.help.hotkeys.ff_speed,
    lang.muxretro.help.hotkeys.ff_audio,
    lang.muxretro.help.hotkeys.ff_glyph,
    lang.muxretro.help.hotkeys.slow_motion,
    lang.muxretro.help.hotkeys.slowmo_speed,
    lang.muxretro.help.hotkeys.slowmo_audio,
    lang.muxretro.help.hotkeys.slowmo_glyph,
    lang.muxretro.help.hotkeys.speed_persistence,
    lang.muxretro.help.hotkeys.pause_content,
    lang.muxretro.help.hotkeys.pause_glyph,
    lang.muxretro.help.hotkeys.quick_save,
    lang.muxretro.help.hotkeys.quick_load,
    lang.muxretro.help.hotkeys.toggle_fps,
    lang.muxretro.help.hotkeys.toggle_header,
    lang.muxretro.help.hotkeys.quit,
    lang.muxretro.help.hotkeys.manual
};

static void enabled_text(char *buf, const size_t buf_len, const int enabled, const char *combo) {
    if (enabled) {
        snprintf(buf, buf_len, lang.muxretro.hotkeys_screen.enabled_combo, lang.generic.enabled, combo);
    } else {
        snprintf(buf, buf_len, "%s", lang.generic.disabled);
    }
}

static void hotkey_mode_text(char *buf, const size_t buf_len, const int mode, const enum hotkey_binding binding) {
    if (mode == hotkey_activation_disabled) {
        snprintf(buf, buf_len, "%s", lang.generic.disabled);
        return;
    }

    char combo[64];
    session_settings_hotkey_combo_name(binding, combo, sizeof(combo));
    snprintf(buf, buf_len, lang.muxretro.hotkeys_screen.mode_combo, session_settings_hotkey_mode_name(mode), combo);
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_button_assignments:
            buf[0] = '\0';
            break;
        case row_ff_enabled:
            hotkey_mode_text(buf, buf_len, session_settings.hotkey_ff_enabled, hotkey_binding_fast_forward);
            break;
        case row_ff_speed:
            snprintf(buf, buf_len, "%s", session_settings_ff_speed_name(session_settings.ff_speed));
            break;
        case row_ff_audio_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_ff_audio_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_ff_glyph_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_ff_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_slowmo_enabled:
            hotkey_mode_text(buf, buf_len, session_settings.hotkey_slowmo_enabled, hotkey_binding_slow_motion);
            break;
        case row_slowmo_speed:
            snprintf(buf, buf_len, "%s", session_settings_slowmo_speed_name(session_settings.slowmo_speed));
            break;
        case row_slowmo_audio_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_slowmo_audio_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_slowmo_glyph_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_slowmo_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_speed_persistence:
            snprintf(
                buf, buf_len, "%s", session_settings.speed_persistence ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_pause_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_pause, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_pause_enabled, combo);
        } break;
        case row_pause_glyph_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_pause_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_quicksave_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_quicksave, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_quicksave_enabled, combo);
        } break;
        case row_quickload_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_quickload, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_quickload_enabled, combo);
        } break;
        case row_toggle_fps_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_toggle_fps, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_toggle_fps_enabled, combo);
        } break;
        case row_header_toggle_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_toggle_header, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_header_toggle_enabled, combo);
        } break;
        case row_quit_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_quit, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_quit_enabled, combo);
        } break;
        case row_manual_enabled: {
            char combo[64];
            session_settings_hotkey_combo_name(hotkey_binding_manual, combo, sizeof(combo));
            enabled_text(buf, buf_len, session_settings.hotkey_manual_enabled, combo);
        } break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_ff_enabled:
            session_settings_cycle_hotkey_ff_enabled(direction);
            break;
        case row_ff_speed:
            session_settings_cycle_ff_speed(direction);
            break;
        case row_ff_audio_enabled:
            session_settings_cycle_hotkey_ff_audio_enabled(direction);
            break;
        case row_ff_glyph_enabled:
            session_settings_cycle_hotkey_ff_glyph_enabled(direction);
            break;
        case row_slowmo_enabled:
            session_settings_cycle_hotkey_slowmo_enabled(direction);
            break;
        case row_slowmo_speed:
            session_settings_cycle_slowmo_speed(direction);
            break;
        case row_slowmo_audio_enabled:
            session_settings_cycle_hotkey_slowmo_audio_enabled(direction);
            break;
        case row_slowmo_glyph_enabled:
            session_settings_cycle_hotkey_slowmo_glyph_enabled(direction);
            break;
        case row_speed_persistence:
            session_settings_cycle_speed_persistence(direction);
            break;
        case row_pause_enabled:
            session_settings_cycle_hotkey_pause_enabled(direction);
            break;
        case row_pause_glyph_enabled:
            session_settings_cycle_hotkey_pause_glyph_enabled(direction);
            break;
        case row_quicksave_enabled:
            session_settings_cycle_hotkey_quicksave_enabled(direction);
            break;
        case row_quickload_enabled:
            session_settings_cycle_hotkey_quickload_enabled(direction);
            break;
        case row_toggle_fps_enabled:
            session_settings_cycle_hotkey_toggle_fps_enabled(direction);
            break;
        case row_header_toggle_enabled:
            session_settings_cycle_hotkey_header_toggle_enabled(direction);
            break;
        case row_quit_enabled:
            session_settings_cycle_hotkey_quit_enabled(direction);
            break;
        case row_manual_enabled:
            session_settings_cycle_hotkey_manual_enabled(direction);
            break;
        default:
            break;
    }
}

static const char *button_labels[hotkey_binding_count] = {
    lang.muxretro.hotkeys_screen.fast_forward,  lang.muxretro.hotkeys_screen.slow_motion,
    lang.muxretro.hotkeys_screen.pause_content, lang.muxretro.hotkeys_screen.quick_save,
    lang.muxretro.hotkeys_screen.quick_load,    lang.muxretro.hotkeys_screen.toggle_fps,
    lang.muxretro.hotkeys_screen.toggle_header, lang.muxretro.quit,
    lang.muxretro.hotkeys_screen.manual
};

static const char *button_glyphs[hotkey_binding_count] = {"fastforward",  "slowmotion", "pause",
                                                          "quicksave",    "quickload",  "togglefps",
                                                          "toggleheader", "quit",       "manual"};

static const char *button_help[hotkey_binding_count] = {
    lang.muxretro.help.hotkeys.button_assignments, lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.button_assignments, lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.button_assignments, lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.button_assignments, lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.button_assignments
};

static void button_value_text(const int index, char *buf, const size_t buf_len) {
    session_settings_hotkey_combo_name((enum hotkey_binding) index, buf, buf_len);
}

#define PICK_COLS 4
#define PICK_MAX  24

static const char *pick_map[PICK_MAX + PICK_MAX / PICK_COLS + 1];
static int pick_buttons[PICK_MAX];
static int pick_count = 0;
static lv_obj_t *pick_grid = NULL;
static int pick_index = 0;
static int pick_active = 0;
static int pick_row = -1;
static uint64_t pick_prev_mask = 0;

static nav_repeat_t rpt_pick_up = {0};
static nav_repeat_t rpt_pick_down = {0};
static nav_repeat_t rpt_pick_left = {0};
static nav_repeat_t rpt_pick_right = {0};

static submenu self;
static submenu buttons_self;

static void pick_build(void) {
    pick_count = 0;
    for (int i = 0; i < session_settings_hotkey_choice_count() && pick_count < PICK_MAX - 1; i++)
        pick_buttons[pick_count++] = session_settings_hotkey_choice(i);
    pick_buttons[pick_count++] = HOTKEY_UNBOUND;

    int slot = 0;
    for (int i = 0; i < pick_count; i++) {
        pick_map[slot++] = pick_buttons[i] == HOTKEY_UNBOUND ? lang.muxretro.settings_screen.unbound
                                                             : mux_input_label((mux_input_type) pick_buttons[i]);
        if ((i + 1) % PICK_COLS == 0 && i + 1 < pick_count) pick_map[slot++] = "\n";
    }
    pick_map[slot] = "";
}

static void pick_select(const int index) {
    if (index < 0 || index >= pick_count) return;
    pick_index = index;

    if (!pick_grid || !lv_obj_is_valid(pick_grid)) return;
    lv_btnmatrix_set_selected_btn(pick_grid, (uint16_t) pick_index);
    lv_btnmatrix_set_btn_ctrl(pick_grid, (uint16_t) pick_index, LV_BTNMATRIX_CTRL_CHECKED);
}

static void pick_move(const int dx, const int dy) {
    const int rows = (pick_count + PICK_COLS - 1) / PICK_COLS;
    int row = pick_index / PICK_COLS;
    int col = pick_index % PICK_COLS;

    if (dx) {
        const int width = row == rows - 1 ? pick_count - row * PICK_COLS : PICK_COLS;
        col = ((col + dx) % width + width) % width;
    } else if (dy) {
        row = ((row + dy) % rows + rows) % rows;
        const int width = row == rows - 1 ? pick_count - row * PICK_COLS : PICK_COLS;
        if (col >= width) col = width - 1;
    }

    pick_select(row * PICK_COLS + col);
}

static void pick_nav_bar(void) {
    nav_show_lr(0);
    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
}

static void pick_open(const int row) {
    pick_row = row;
    pick_active = 1;

    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;
    first_open = 0;

    pick_build();

    pick_grid = lv_btnmatrix_create(ui_pnl_content);
    lv_obj_set_width(pick_grid, lv_pct(92));
    lv_obj_set_height(pick_grid, lv_pct(72));

    lv_btnmatrix_set_one_checked(pick_grid, 1);
    lv_btnmatrix_set_map(pick_grid, pick_map);
    lv_obj_clear_flag(pick_grid, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_flag(pick_grid, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(pick_grid, LV_ALIGN_CENTER, 0, 0);

    apply_osk_theme(pick_grid);

    lv_shadow_zone_register(
        pick_grid, lv_color_hex(theme.osk.item.shadow_colour), (lv_opa_t) theme.osk.item.shadow_alpha,
        (int8_t) theme.osk.item.shadow_x_offset, (int8_t) theme.osk.item.shadow_y_offset,
        lv_color_hex(theme.osk.item.shadow_colour_focus), (lv_opa_t) theme.osk.item.shadow_alpha_focus,
        (int8_t) theme.osk.item.shadow_x_offset_focus, (int8_t) theme.osk.item.shadow_y_offset_focus
    );

    const int current = session_settings_hotkey_button((enum hotkey_binding) row);
    pick_index = pick_count - 1;
    for (int i = 0; i < pick_count; i++) {
        if (pick_buttons[i] == current) {
            pick_index = i;
            break;
        }
    }

    pick_select(pick_index);

    pick_prev_mask = nav_mask_standard();
    pick_nav_bar();
    pause_menu_sync_input_mask();
}

static void pick_close(void) {
    pick_active = 0;

    if (pick_grid && lv_obj_is_valid(pick_grid)) lv_shadow_zone_unregister(pick_grid);
    pick_grid = NULL;
    submenu_reopen_at(&buttons_self, pick_row);
}

static void pick_apply(void) {
    const int button = pick_buttons[pick_index];
    if (button == session_settings_hotkey_button((enum hotkey_binding) pick_row)) return;

    const int displaced = session_settings_set_hotkey_button((enum hotkey_binding) pick_row, button);
    if (displaced < 0) return;

    char message[256];
    snprintf(message, sizeof(message), lang.muxretro.hotkeys_screen.unbound_combo, button_labels[displaced]);
    pause_menu_show_toast(message);
}

static void pick_tick(void) {
    const uint64_t mask = nav_mask_standard();
    const uint64_t edge = mask & ~pick_prev_mask;
    pick_prev_mask = mask;

    if (nav_input_halted()) return;

    const uint32_t now = SDL_GetTicks();
    const int do_up = nav_repeat_step(&rpt_pick_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int do_down = nav_repeat_step(&rpt_pick_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int do_left = nav_repeat_step(&rpt_pick_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int do_right = nav_repeat_step(&rpt_pick_right, edge & BIT(3), mask & BIT(3), 1, now);

    if (do_up || do_down || do_left || do_right) {
        play_sound(snd_navigate);
        pick_move(do_right - do_left, do_down - do_up);
    } else if (edge & BIT(4)) {
        play_sound(snd_confirm);
        pick_apply();
        pick_close();
    } else if (edge & BIT(5)) {
        play_sound(snd_back);
        pick_close();
    }
}

static int button_row_is_action(const int index) {
    (void) index;
    return 1;
}

static int button_row_can_cycle(const int index) {
    return index >= 0 && index < hotkey_binding_count;
}

static void button_cycle(const int index, const int direction) {
    (void) direction;
    const enum hotkey_binding binding = (enum hotkey_binding) index;
    const int displaced = session_settings_set_hotkey_direct(binding, !session_settings_hotkey_direct(binding));
    if (displaced < 0) return;

    char message[256];
    snprintf(message, sizeof(message), lang.muxretro.hotkeys_screen.unbound_combo, button_labels[displaced]);
    pause_menu_show_toast(message);
}

static void button_action(const int index) {
    if (index < 0 || index >= hotkey_binding_count) return;
    pick_open(index);
}

static int button_row_depends_on(const int index, const int changed_index) {
    return index != changed_index;
}

static const char *button_extra_label(const int index) {
    (void) index;
    return lang.generic.reset;
}

static void reset_buttons(const int index) {
    (void) index;
    session_settings_reset_hotkey_buttons();
    submenu_refresh_values(&buttons_self);
}

static void buttons_closed(void) {
    submenu_reopen_at(&self, row_button_assignments);
}

static const submenu_def buttons_def = {
    .labels = button_labels,
    .glyphs = button_glyphs,
    .help = button_help,
    .row_count = hotkey_binding_count,
    .value_text = button_value_text,
    .cycle = button_cycle,
    .row_can_cycle = button_row_can_cycle,
    .row_is_action = button_row_is_action,
    .action = button_action,
    .row_depends_on = button_row_depends_on,
    .extra_label = button_extra_label,
    .extra_action = reset_buttons,
    .closed = buttons_closed,
    .save_title = lang.muxretro.save.hotkeys_title,
    .save_desc = lang.muxretro.save.hotkeys_desc,
};

static int row_is_action(const int index) {
    return index == row_button_assignments;
}

static int row_can_cycle(const int index) {
    if (index == row_button_assignments) return 0;
    if ((index == row_ff_speed || index == row_ff_audio_enabled || index == row_ff_glyph_enabled)
        && session_settings.hotkey_ff_enabled == hotkey_activation_disabled)
        return 0;
    if ((index == row_slowmo_speed || index == row_slowmo_audio_enabled || index == row_slowmo_glyph_enabled)
        && session_settings.hotkey_slowmo_enabled == hotkey_activation_disabled)
        return 0;
    if (index == row_pause_glyph_enabled && !session_settings.hotkey_pause_enabled) return 0;
    return 1;
}

static int row_depends_on(const int index, const int changed_index) {
    if (changed_index == row_ff_enabled)
        return index == row_ff_speed || index == row_ff_audio_enabled || index == row_ff_glyph_enabled;
    if (changed_index == row_slowmo_enabled)
        return index == row_slowmo_speed || index == row_slowmo_audio_enabled || index == row_slowmo_glyph_enabled;
    if (changed_index == row_pause_enabled) return index == row_pause_glyph_enabled;
    return 0;
}

static void row_action(const int index) {
    if (index == row_button_assignments) submenu_open(&buttons_self);
}

static int child_tick(void) {
    if (pick_active) {
        pick_tick();
        return 1;
    }
    if (!submenu_is_active(&buttons_self)) return 0;
    submenu_tick(&buttons_self);
    return 1;
}

static void closed(void) {
    input_menu_reopen_hotkeys();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_can_cycle = row_can_cycle,
    .row_depends_on = row_depends_on,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.hotkeys_title,
    .save_desc = lang.muxretro.save.hotkeys_desc,
};

void hotkeys_menu_init(void) {
    submenu_init(&self, &def);
    submenu_init(&buttons_self, &buttons_def);
}

void hotkeys_menu_open(void) {
    submenu_open(&self);
}

int hotkeys_menu_is_active(void) {
    return submenu_is_active(&self);
}

void hotkeys_menu_tick(void) {
    submenu_tick(&self);
}
