#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_button_assignments = 0,
    row_ff_enabled,
    row_ff_speed,
    row_ff_glyph_enabled,
    row_slowmo_enabled,
    row_slowmo_speed,
    row_slowmo_glyph_enabled,
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
    lang.muxretro.hotkeys_screen.ff_glyph,
    lang.muxretro.hotkeys_screen.slow_motion,
    lang.muxretro.hotkeys_screen.slowmo_speed,
    lang.muxretro.hotkeys_screen.slowmo_glyph,
    lang.muxretro.hotkeys_screen.pause_content,
    lang.muxretro.hotkeys_screen.pause_glyph,
    lang.muxretro.hotkeys_screen.quick_save,
    lang.muxretro.hotkeys_screen.quick_load,
    lang.muxretro.hotkeys_screen.toggle_fps,
    lang.muxretro.hotkeys_screen.toggle_header,
    lang.muxretro.quit,
    lang.muxretro.hotkeys_screen.manual
};

static const char *row_glyphs[row_count] = {"hotkeys",     "fastforward", "ffspeed",      "ffglyph",    "slowmotion",
                                            "slowmospeed", "slowmoglyph", "pause",        "pauseglyph", "quicksave",
                                            "quickload",   "togglefps",   "toggleheader", "quit",       "manual"};

static const char *row_help[row_count] = {
    lang.muxretro.help.hotkeys.button_assignments,
    lang.muxretro.help.hotkeys.fast_forward,
    lang.muxretro.help.hotkeys.ff_speed,
    lang.muxretro.help.hotkeys.ff_glyph,
    lang.muxretro.help.hotkeys.slow_motion,
    lang.muxretro.help.hotkeys.slowmo_speed,
    lang.muxretro.help.hotkeys.slowmo_glyph,
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
        case row_slowmo_glyph_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_slowmo_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
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
        case row_ff_glyph_enabled:
            session_settings_cycle_hotkey_ff_glyph_enabled(direction);
            break;
        case row_slowmo_enabled:
            session_settings_cycle_hotkey_slowmo_enabled(direction);
            break;
        case row_slowmo_speed:
            session_settings_cycle_slowmo_speed(direction);
            break;
        case row_slowmo_glyph_enabled:
            session_settings_cycle_hotkey_slowmo_glyph_enabled(direction);
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

static void cycle_button(const int index, const int direction) {
    const int displaced = session_settings_hotkey_button((enum hotkey_binding) index);
    session_settings_cycle_hotkey_button((enum hotkey_binding) index, direction);
    for (int other = 0; other < hotkey_binding_count; other++) {
        if (other == index || session_settings_hotkey_button((enum hotkey_binding) other) != displaced) continue;

        char message[256];
        char combo[64];
        session_settings_hotkey_combo_name((enum hotkey_binding) other, combo, sizeof(combo));
        snprintf(message, sizeof(message), lang.muxretro.hotkeys_screen.reassigned_combo, button_labels[other], combo);
        pause_menu_show_toast(message);
        break;
    }
}

static submenu self;
static submenu buttons_self;

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
    .cycle = cycle_button,
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
    if ((index == row_ff_speed || index == row_ff_glyph_enabled)
        && session_settings.hotkey_ff_enabled == hotkey_activation_disabled)
        return 0;
    if ((index == row_slowmo_speed || index == row_slowmo_glyph_enabled)
        && session_settings.hotkey_slowmo_enabled == hotkey_activation_disabled)
        return 0;
    if (index == row_pause_glyph_enabled && !session_settings.hotkey_pause_enabled) return 0;
    return 1;
}

static void row_action(const int index) {
    if (index == row_button_assignments) submenu_open(&buttons_self);
}

static int child_tick(void) {
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
