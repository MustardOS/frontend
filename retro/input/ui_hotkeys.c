#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_ff_enabled = 0,
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

static const char *row_glyphs[row_count] = {"fastforward", "ffspeed",      "ffglyph",    "slowmotion", "slowmospeed",
                                            "slowmoglyph", "pause",        "pauseglyph", "quicksave",  "quickload",
                                            "togglefps",   "toggleheader", "quit",       "manual"};

static const char *row_help[row_count] = {
    lang.muxretro.help.hotkeys.fast_forward,  lang.muxretro.help.hotkeys.ff_speed,
    lang.muxretro.help.hotkeys.ff_glyph,      lang.muxretro.help.hotkeys.slow_motion,
    lang.muxretro.help.hotkeys.slowmo_speed,  lang.muxretro.help.hotkeys.slowmo_glyph,
    lang.muxretro.help.hotkeys.pause_content, lang.muxretro.help.hotkeys.pause_glyph,
    lang.muxretro.help.hotkeys.quick_save,    lang.muxretro.help.hotkeys.quick_load,
    lang.muxretro.help.hotkeys.toggle_fps,    lang.muxretro.help.hotkeys.toggle_header,
    lang.muxretro.help.hotkeys.quit,          lang.muxretro.help.hotkeys.manual
};

static void enabled_text(char *buf, const size_t buf_len, const int enabled, const char *combo) {
    if (enabled) {
        snprintf(buf, buf_len, "%s (%s)", lang.generic.enabled, combo);
    } else {
        snprintf(buf, buf_len, "%s", lang.generic.disabled);
    }
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_ff_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_ff_enabled, "M+R1");
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
            enabled_text(buf, buf_len, session_settings.hotkey_slowmo_enabled, "M+L1");
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
        case row_pause_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_pause_enabled, "M+B");
            break;
        case row_pause_glyph_enabled:
            snprintf(
                buf, buf_len, "%s",
                session_settings.hotkey_pause_glyph_enabled ? lang.generic.enabled : lang.generic.disabled
            );
            break;
        case row_quicksave_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_quicksave_enabled, "M+R2");
            break;
        case row_quickload_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_quickload_enabled, "M+L2");
            break;
        case row_toggle_fps_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_toggle_fps_enabled, "M+Y");
            break;
        case row_header_toggle_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_header_toggle_enabled, "M+X");
            break;
        case row_quit_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_quit_enabled, "M+START");
            break;
        case row_manual_enabled:
            enabled_text(buf, buf_len, session_settings.hotkey_manual_enabled, "M+SELECT");
            break;
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

static void closed(void) {
    input_menu_reopen_hotkeys();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.hotkeys_title,
    .save_desc = lang.muxretro.save.hotkeys_desc,
};

void hotkeys_menu_init(void) {
    submenu_init(&self, &def);
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
