#include <stdio.h>
#include "../common/init.h"
#include "../common/input.h"
#include "../common/language.h"
#include "../common/log.h"
#include "gamestate.h"
#include "hotkeys.h"
#include "muxretro.h"
#include "settings.h"

static int menu_held = 0;
static int menu_chord_consumed = 0;

static int prev_r1 = 0;
static int prev_r2 = 0;
static int prev_l1 = 0;
static int prev_l2 = 0;

static int fast_forward_active = 0;
static int slow_motion_active = 0;

int hotkeys_is_fast_forward_active(void) {
    return fast_forward_active;
}

int hotkeys_is_slow_motion_active(void) {
    return slow_motion_active;
}

static void sync_audio_mute(void) {
    audio_bridge_set_muted(fast_forward_active || slow_motion_active);
    audio_bridge_clear_queued();
}

static void sync_speed_indicator(void) {
    if (fast_forward_active) {
        pause_menu_set_speed_indicator("FF");
    } else if (slow_motion_active) {
        pause_menu_set_speed_indicator("SM");
    } else {
        pause_menu_set_speed_indicator(NULL);
    }
}

static void toggle_fast_forward(void) {
    fast_forward_active = !fast_forward_active;
    if (fast_forward_active) slow_motion_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Fast Forward %s (hotkey)", fast_forward_active ? "enabled" : "disabled");

    if (fast_forward_active) {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s: %s (M+R1)", lang.muxretro.hotkeys_screen.fast_forward, lang.generic.enabled);
        pause_menu_show_toast(msg);
    }
}

static void toggle_slow_motion(void) {
    slow_motion_active = !slow_motion_active;
    if (slow_motion_active) fast_forward_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Slow Motion %s (hotkey)", slow_motion_active ? "enabled" : "disabled");

    if (slow_motion_active) {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s: %s (M+L1)", lang.muxretro.hotkeys_screen.slow_motion, lang.generic.enabled);
        pause_menu_show_toast(msg);
    }
}

void hotkeys_reset(void) {
    if (!fast_forward_active && !slow_motion_active) return;
    fast_forward_active = 0;
    slow_motion_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
}

int hotkeys_task(void) {
    const int menu_now = mux_input_pressed(mux_input_menu);
    const int r1_now = mux_input_pressed(mux_input_r1);
    const int r2_now = mux_input_pressed(mux_input_r2);
    const int l1_now = mux_input_pressed(mux_input_l1);
    const int l2_now = mux_input_pressed(mux_input_l2);

    int open_pause = 0;

    if (!menu_held && menu_now) {
        menu_held = 1;
        menu_chord_consumed = 0;
    } else if (menu_held && !menu_now) {
        menu_held = 0;
        open_pause = !menu_chord_consumed;
    }

    if (menu_held) {
        if (r1_now && !prev_r1 && session_settings.hotkey_ff_enabled) {
            toggle_fast_forward();
            input_bridge_suppress(mux_input_r1);
            menu_chord_consumed = 1;
        }

        if (r2_now && !prev_r2 && session_settings.hotkey_quicksave_enabled) {
            gamestate_quicksave_save();
            LOG_INFO(mux_module, "Quick Save (hotkey)");
            pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_save);
            input_bridge_suppress(mux_input_r2);
            menu_chord_consumed = 1;
        }

        if (l1_now && !prev_l1 && session_settings.hotkey_slowmo_enabled) {
            toggle_slow_motion();
            input_bridge_suppress(mux_input_l1);
            menu_chord_consumed = 1;
        }

        if (l2_now && !prev_l2 && session_settings.hotkey_quickload_enabled) {
            if (gamestate_quicksave_load() == 0) {
                LOG_INFO(mux_module, "Quick Load (hotkey)");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_load);
            } else {
                LOG_INFO(mux_module, "Quick Load (hotkey): no quicksave to load");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.no_quicksave);
            }
            input_bridge_suppress(mux_input_l2);
            menu_chord_consumed = 1;
        }
    }

    prev_r1 = r1_now;
    prev_r2 = r2_now;
    prev_l1 = l1_now;
    prev_l2 = l2_now;

    return open_pause;
}
