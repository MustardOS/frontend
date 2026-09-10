#include <stdio.h>
#include <common/runtime/init.h>
#include <common/platform/input.h>
#include <common/display/language.h>
#include <common/runtime/log.h>
#include <common/ui/common.h>
#include <common/ui/nav.h>
#include "../state/gamestate.h"
#include "../state/manual.h"
#include "../link/link.h"
#include "../netplay/netplay.h"
#include "hotkeys.h"
#include "../core/muxretro.h"
#include "nav_repeat.h"
#include "../settings/settings.h"

#define TOAST_FOCUS_MS 1200

static int menu_held = 0;
static int menu_combo_consumed = 0;

static int prev_a = 0;
static int prev_hotkey[hotkey_binding_count];

static int fast_forward_active = 0;
static int slow_motion_active = 0;
static int content_paused = 0;
static int quit_requested = 0;
static int manual_requested = 0;
static int held_speed = 0;

int hotkeys_is_fast_forward_active(void) {
    return fast_forward_active;
}

int hotkeys_is_slow_motion_active(void) {
    return slow_motion_active;
}

int hotkeys_is_content_paused(void) {
    return content_paused;
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
    const int should_mute = content_paused || fast_forward_active || slow_motion_active;
    const int was_muted = audio_bridge_is_muted();

    audio_bridge_set_muted(should_mute);
    audio_bridge_clear_queued();

    if (was_muted && !should_mute) core_prime_audio();
}

static void sync_speed_indicator(void) {
    if (content_paused && session_settings.hotkey_pause_glyph_enabled) {
        pause_menu_set_speed_indicator(lang.muxretro.hotkeys_screen.paused, "pause");
    } else if (fast_forward_active && session_settings.hotkey_ff_glyph_enabled) {
        pause_menu_set_speed_indicator(session_settings_ff_speed_name(session_settings.ff_speed), "fastforward");
    } else if (slow_motion_active && session_settings.hotkey_slowmo_glyph_enabled) {
        pause_menu_set_speed_indicator(session_settings_slowmo_speed_name(session_settings.slowmo_speed), "slowmotion");
    } else {
        pause_menu_set_speed_indicator(NULL, NULL);
    }
}

static void set_fast_forward(const int active) {
    if (fast_forward_active == active) return;
    fast_forward_active = active;
    if (fast_forward_active) slow_motion_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Fast Forward %s (hotkey)", fast_forward_active ? "enabled" : "disabled");
}

static void set_slow_motion(const int active) {
    if (slow_motion_active == active) return;
    slow_motion_active = active;
    if (slow_motion_active) fast_forward_active = 0;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Slow Motion %s (hotkey)", slow_motion_active ? "enabled" : "disabled");
}

static void toggle_fast_forward(void) {
    set_fast_forward(!fast_forward_active);
}

static void toggle_slow_motion(void) {
    set_slow_motion(!slow_motion_active);
}

static void toggle_content_pause(void) {
    content_paused = !content_paused;
    sync_audio_mute();
    sync_speed_indicator();
    LOG_INFO(mux_module, "Content Pause %s (hotkey)", content_paused ? "enabled" : "disabled");
}

void hotkeys_reset(void) {
    if (!fast_forward_active && !slow_motion_active && !content_paused) return;
    fast_forward_active = 0;
    slow_motion_active = 0;
    held_speed = 0;
    content_paused = 0;
    sync_audio_mute();
    sync_speed_indicator();
}

int hotkeys_task(void) {
    const int menu_now = mux_input_pressed(mux_input_menu);
    const int a_now = mux_input_pressed(mux_input_a);

    int hotkey_now[hotkey_binding_count];
    for (int binding = 0; binding < hotkey_binding_count; binding++) {
        hotkey_now[binding] =
            mux_input_pressed((mux_input_type) session_settings_hotkey_button((enum hotkey_binding) binding));
    }

    int open_pause = 0;

    if (!menu_held && menu_now) {
        menu_held = 1;
        menu_combo_consumed = 0;
    } else if (menu_held && !menu_now) {
        menu_held = 0;
        open_pause = !menu_combo_consumed;
    }

    const int speed_allowed = !netplay_is_active();
    const int ff_hold = speed_allowed && menu_held && session_settings.hotkey_ff_enabled == hotkey_activation_hold
                        && hotkey_now[hotkey_binding_fast_forward];
    const int slow_hold = speed_allowed && menu_held && session_settings.hotkey_slowmo_enabled == hotkey_activation_hold
                          && hotkey_now[hotkey_binding_slow_motion];

    int wanted_held_speed = 0;
    if (ff_hold && slow_hold) {
        const int ff_new = !prev_hotkey[hotkey_binding_fast_forward];
        const int slow_new = !prev_hotkey[hotkey_binding_slow_motion];
        if (ff_new != slow_new)
            wanted_held_speed = ff_new ? 1 : 2;
        else if (held_speed == 1 || held_speed == 2)
            wanted_held_speed = held_speed;
        else
            wanted_held_speed = 1;
    } else if (ff_hold) {
        wanted_held_speed = 1;
    } else if (slow_hold) {
        wanted_held_speed = 2;
    }

    if (wanted_held_speed != held_speed) {
        held_speed = wanted_held_speed;
        if (held_speed == 1) {
            set_fast_forward(1);
        } else if (held_speed == 2) {
            set_slow_motion(1);
        } else {
            set_fast_forward(0);
            set_slow_motion(0);
        }
    }

    if (wanted_held_speed) {
        const enum hotkey_binding binding =
            wanted_held_speed == 1 ? hotkey_binding_fast_forward : hotkey_binding_slow_motion;
        input_bridge_suppress((mux_input_type) session_settings_hotkey_button(binding));
        menu_combo_consumed = 1;
    }

    if (menu_held) {
        if (hotkey_now[hotkey_binding_fast_forward] && !prev_hotkey[hotkey_binding_fast_forward]
            && session_settings.hotkey_ff_enabled == hotkey_activation_press && speed_allowed && !held_speed) {
            toggle_fast_forward();
            input_bridge_suppress((mux_input_type) session_settings.hotkey_ff_button);
            menu_combo_consumed = 1;
        }

        if (a_now && !prev_a && link_local_active()) {
            link_toggle_focus();
            input_bridge_suppress(mux_input_a);
            menu_combo_consumed = 1;

            pause_menu_refresh_gb_slot();

            if (!link_single_screen()) {
                char focus_message[64];
                snprintf(focus_message, sizeof(focus_message), lang.muxretro.link.focus_moved, link_get_focus() + 1);
                pause_menu_show_glyph_toast_timed(focus_message, "controller", TOAST_FOCUS_MS);
            }
        }

        if (hotkey_now[hotkey_binding_quicksave] && !prev_hotkey[hotkey_binding_quicksave]
            && session_settings.hotkey_quicksave_enabled && state_saves_allowed()) {
            if (state_saves_supported()) {
                if (gamestate_quicksave_save() == 0) {
                    LOG_INFO(mux_module, "Quick Save (hotkey)");
                    pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_save);
                } else {
                    LOG_WARN(mux_module, "Quick Save (hotkey) failed");
                    pause_menu_show_toast(lang.generic.save_fail);
                }
            } else {
                pause_menu_show_toast(lang.muxretro.gamestate.not_supported);
            }
            input_bridge_suppress((mux_input_type) session_settings.hotkey_quicksave_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_slow_motion] && !prev_hotkey[hotkey_binding_slow_motion]
            && session_settings.hotkey_slowmo_enabled == hotkey_activation_press && speed_allowed && !held_speed) {
            toggle_slow_motion();
            input_bridge_suppress((mux_input_type) session_settings.hotkey_slowmo_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_pause] && !prev_hotkey[hotkey_binding_pause]
            && session_settings.hotkey_pause_enabled && !netplay_is_active()) {
            toggle_content_pause();
            input_bridge_suppress((mux_input_type) session_settings.hotkey_pause_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_quickload] && !prev_hotkey[hotkey_binding_quickload]
            && session_settings.hotkey_quickload_enabled && state_saves_allowed()) {
            if (!state_saves_supported()) {
                pause_menu_show_toast(lang.muxretro.gamestate.not_supported);
            } else if (gamestate_quicksave_exists && !gamestate_metadata_matches(&gamestate_quicksave)) {
                LOG_INFO(mux_module, "Quick Load (hotkey): blocked, quicksave metadata mismatch");
                pause_menu_toggle();
                gamestate_notice_open();
            } else if (gamestate_quicksave_load() == 0) {
                LOG_INFO(mux_module, "Quick Load (hotkey)");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.quick_load);
            } else {
                LOG_INFO(mux_module, "Quick Load (hotkey): no quicksave to load");
                pause_menu_show_toast(lang.muxretro.hotkeys_screen.no_quicksave);
            }
            input_bridge_suppress((mux_input_type) session_settings.hotkey_quickload_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_toggle_fps] && !prev_hotkey[hotkey_binding_toggle_fps]
            && session_settings.hotkey_toggle_fps_enabled) {
            session_settings_cycle_fps(0);
            LOG_INFO(mux_module, "Toggle FPS %s (hotkey)", session_settings_show_fps_name(session_settings.show_fps));
            input_bridge_suppress((mux_input_type) session_settings.hotkey_toggle_fps_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_toggle_header] && !prev_hotkey[hotkey_binding_toggle_header]
            && session_settings.hotkey_header_toggle_enabled) {
            session_settings_cycle_header_visibility(1);
            pause_menu_apply_header_visibility();
            LOG_INFO(
                mux_module, "Header Visibility: %s (hotkey)",
                session_settings_header_visibility_name(session_settings.header_visibility)
            );

            input_bridge_suppress((mux_input_type) session_settings.hotkey_header_toggle_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_quit] && !prev_hotkey[hotkey_binding_quit]
            && session_settings.hotkey_quit_enabled) {
            if (state_saves_allowed() && session_settings_auto_save_on_quit()) gamestate_autosave_save();
            LOG_INFO(mux_module, "Quit (hotkey)");
            quit_requested = 1;
            input_bridge_suppress((mux_input_type) session_settings.hotkey_quit_button);
            menu_combo_consumed = 1;
        }

        if (hotkey_now[hotkey_binding_manual] && !prev_hotkey[hotkey_binding_manual]
            && session_settings.hotkey_manual_enabled) {
            if (manual_is_available()) {
                LOG_INFO(mux_module, "Manual (hotkey)");
                manual_requested = 1;
            } else {
                pause_menu_show_toast(lang.muxretro.manual_screen.not_found);
            }
            input_bridge_suppress((mux_input_type) session_settings.hotkey_manual_button);
            menu_combo_consumed = 1;
        }
    }

    for (int binding = 0; binding < hotkey_binding_count; binding++)
        prev_hotkey[binding] = hotkey_now[binding];
    prev_a = a_now;

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
