#include <stdio.h>
#include "../../common/init.h"
#include "../../common/input.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/ui/common.h"
#include "../state/gamestate.h"
#include "../state/manual.h"
#include "hotkeys.h"
#include "../core/muxretro.h"
#include "nav_repeat.h"
#include "../settings/settings.h"

static int menu_held = 0;
static int menu_combo_consumed = 0;

static int prev_a = 0;
static int prev_r1 = 0;
static int prev_r2 = 0;
static int prev_l1 = 0;
static int prev_l2 = 0;
static int prev_y = 0;
static int prev_x = 0;
static int prev_start = 0;
static int prev_select = 0;

static int fast_forward_active = 0;
static int slow_motion_active = 0;
static int quit_requested = 0;
static int manual_requested = 0;

int hotkeys_is_fast_forward_active(void) {
    return fast_forward_active;
}

int hotkeys_is_slow_motion_active(void) {
    return slow_motion_active;
}

int hotkeys_is_quit_requested(void) {
    return quit_requested;
}

void hotkeys_request_quit(void) {
    quit_requested = 1;
}

int hotkeys_is_manual_requested(void) {
    const int r = manual_requested;
    manual_requested = 0;
    return r;
}

static void sync_audio_mute(void) {
    const int should_mute = fast_forward_active || slow_motion_active;
    const int was_muted = audio_bridge_is_muted();

    audio_bridge_set_muted(should_mute);
    audio_bridge_clear_queued();

    if (was_muted && !should_mute) core_prime_audio();
}

static void sync_speed_indicator(void) {
    if (fast_forward_active && session_settings.hotkey_ff_glyph_enabled) {
        pause_menu_set_speed_indicator(session_settings_ff_speed_name(session_settings.ff_speed), "fastforward");
    } else if (slow_motion_active && session_settings.hotkey_slowmo_glyph_enabled) {
        pause_menu_set_speed_indicator(session_settings_slowmo_speed_name(session_settings.slowmo_speed), "slowmotion");
    } else {
        pause_menu_set_speed_indicator(NULL, NULL);
    }
}

static void toggle_fast_forward(void) {
    fast_forward_active = !fast_forward_active;
    if (fast_forward_active) slow_motion_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Fast Forward %s (hotkey)", fast_forward_active ? "enabled" : "disabled");
}

static void toggle_slow_motion(void) {
    slow_motion_active = !slow_motion_active;
    if (slow_motion_active) fast_forward_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Slow Motion %s (hotkey)", slow_motion_active ? "enabled" : "disabled");
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
    const int y_now = mux_input_pressed(mux_input_y);
    const int x_now = mux_input_pressed(mux_input_x);
    const int start_now = mux_input_pressed(mux_input_start);
    const int a_now = mux_input_pressed(mux_input_a);
    const int select_now = mux_input_pressed(mux_input_select);

    int open_pause = 0;

    if (!menu_held && menu_now) {
        menu_held = 1;
        menu_combo_consumed = 0;
    } else if (menu_held && !menu_now) {
        menu_held = 0;
        open_pause = !menu_combo_consumed;
    }

    if (menu_held) {
        if (r1_now && !prev_r1 && session_settings.hotkey_ff_enabled) {
            toggle_fast_forward();
            input_bridge_suppress(mux_input_r1);
            menu_combo_consumed = 1;
        }

        if (r2_now && !prev_r2 && session_settings.hotkey_quicksave_enabled) {
            if (state_saves_supported()) {
                gamestate_quicksave_save();
                LOG_INFO(mux_module, "Quick Save (hotkey)");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_save);
            } else {
                pause_menu_show_toast(lang.muxretro.gamestate.not_supported);
            }
            input_bridge_suppress(mux_input_r2);
            menu_combo_consumed = 1;
        }

        if (l1_now && !prev_l1 && session_settings.hotkey_slowmo_enabled) {
            toggle_slow_motion();
            input_bridge_suppress(mux_input_l1);
            menu_combo_consumed = 1;
        }

        if (l2_now && !prev_l2 && session_settings.hotkey_quickload_enabled) {
            if (!state_saves_supported()) {
                pause_menu_show_toast(lang.muxretro.gamestate.not_supported);
            } else if (gamestate_quicksave_load() == 0) {
                LOG_INFO(mux_module, "Quick Load (hotkey)");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_load);
            } else {
                LOG_INFO(mux_module, "Quick Load (hotkey): no quicksave to load");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.no_quicksave);
            }
            input_bridge_suppress(mux_input_l2);
            menu_combo_consumed = 1;
        }

        if (y_now && !prev_y && session_settings.hotkey_toggle_fps_enabled) {
            session_settings_cycle_fps(0);
            LOG_INFO(mux_module, "Toggle FPS %s (hotkey)", session_settings.show_fps ? "enabled" : "disabled");
            input_bridge_suppress(mux_input_y);
            menu_combo_consumed = 1;
        }

        if (x_now && !prev_x && session_settings.hotkey_header_toggle_enabled) {
            session_settings_cycle_header_visibility(1);
            pause_menu_apply_header_visibility();
            LOG_INFO(
                mux_module, "Header Visibility: %s (hotkey)",
                session_settings_header_visibility_name(session_settings.header_visibility)
            );

            input_bridge_suppress(mux_input_x);
            menu_combo_consumed = 1;
        }

        if (a_now && !prev_a && session_settings.hotkey_analog_toggle_enabled) {
            session_settings_cycle_analog_controller(0);

            char toast[MAX_BUFFER_SIZE];
            snprintf(
                toast, sizeof(toast), "%s: %s", lang.muxretro.settings_screen.controller_type,
                session_settings.analog_controller ? lang.muxretro.settings_screen.controller_analog
                                                   : lang.muxretro.settings_screen.controller_digital
            );
            pause_menu_show_toast(toast);

            LOG_INFO(
                mux_module, "Controller Type %s (hotkey)", session_settings.analog_controller ? "analog" : "digital"
            );
            input_bridge_suppress(mux_input_a);
            menu_combo_consumed = 1;
        }

        if (start_now && !prev_start && session_settings.hotkey_quit_enabled) {
            if (session_settings_auto_save_on_quit()) gamestate_autosave_save();
            LOG_INFO(mux_module, "Quit (hotkey)");
            quit_requested = 1;
            input_bridge_suppress(mux_input_start);
            menu_combo_consumed = 1;
        }

        if (select_now && !prev_select && session_settings.hotkey_manual_enabled) {
            if (manual_is_available()) {
                LOG_INFO(mux_module, "Manual (hotkey)");
                manual_requested = 1;
            } else {
                pause_menu_show_toast(lang.muxretro.manual_screen.not_found);
            }
            input_bridge_suppress(mux_input_select);
            menu_combo_consumed = 1;
        }
    }

    prev_a = a_now;
    prev_r1 = r1_now;
    prev_r2 = r2_now;
    prev_l1 = l1_now;
    prev_l2 = l2_now;
    prev_y = y_now;
    prev_x = x_now;
    prev_start = start_now;
    prev_select = select_now;

    return open_pause;
}

static nav_repeat_t rpt_vol_up = {0};
static nav_repeat_t rpt_vol_down = {0};

static int prev_vol_up = 0;
static int prev_vol_down = 0;

void hotkeys_volume_bright_task(void) {
    const int bright_mod = mux_input_pressed(mux_input_menu) || mux_input_pressed(mux_input_switch);

    const int vol_up_now = mux_input_pressed(mux_input_vol_up);
    const int vol_down_now = mux_input_pressed(mux_input_vol_down);

    const uint32_t now = SDL_GetTicks();

    if (nav_repeat_step(&rpt_vol_up, vol_up_now && !prev_vol_up, vol_up_now, 1, now)) {
        if (bright_mod) {
            ui_common_handle_bright_up();
        } else {
            ui_common_handle_volume_up();
        }
        ui_common_progress_tick();
        if (menu_held) menu_combo_consumed = 1;
    }

    if (nav_repeat_step(&rpt_vol_down, vol_down_now && !prev_vol_down, vol_down_now, 1, now)) {
        if (bright_mod) {
            ui_common_handle_bright_down();
        } else {
            ui_common_handle_volume_down();
        }
        ui_common_progress_tick();
        if (menu_held) menu_combo_consumed = 1;
    }

    prev_vol_up = vol_up_now;
    prev_vol_down = vol_down_now;
}
