#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../core/perf.h"
#include "../coreinfo/coreinfo.h"
#include "settings.h"
#include "submenu.h"

enum {
    row_preset = 0,
    row_fps_limit,
    row_frame_delay,
    row_run_ahead,
    row_gpu_hard_sync,
    row_smoothness_check,
    row_performance_capture,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.performance_preset, lang.muxretro.settings_screen.fps_limit,
    lang.muxretro.settings_screen.frame_delay,        lang.muxretro.settings_screen.run_ahead,
    lang.muxretro.settings_screen.gpu_hard_sync,      lang.muxretro.settings_screen.smoothness_check,
    lang.muxretro.settings_screen.performance_capture
};

static const char *row_glyphs[row_count] = {
    "performance", "fpslimit", "framedelay", "runahead", "hardsync", "performance", "info"
};

static const char *row_help[row_count] = {
    lang.muxretro.help.performance.preset,        lang.muxretro.help.performance.fps_limit,
    lang.muxretro.help.performance.frame_delay,   lang.muxretro.help.performance.run_ahead,
    lang.muxretro.help.performance.gpu_hard_sync, lang.muxretro.help.performance.smoothness,
    lang.muxretro.help.performance.capture
};

static int advisor_capture_owned;
static int advisor_result_shown;

enum { preset_recommended = 0, preset_low_latency, preset_stable, preset_count };

static const struct {
    const char *name;
    int frame_delay_ms;
    int run_ahead;
    int gpu_hard_sync;
} presets[preset_count] = {
    {lang.muxretro.settings_screen.preset_recommended, FRAME_DELAY_AUTO, 0, 0},
    {lang.muxretro.settings_screen.preset_low_latency, FRAME_DELAY_AUTO, 1, 0},
    {lang.muxretro.settings_screen.preset_stable, FRAME_DELAY_OFF, 0, 0}
};

static int current_preset(void) {
    for (int i = 0; i < preset_count; i++)
        if (session_settings.frame_delay_ms == presets[i].frame_delay_ms
            && !session_settings.run_ahead == !presets[i].run_ahead
            && !session_settings.gpu_hard_sync == !presets[i].gpu_hard_sync)
            return i;

    return -1;
}

static void apply_preset(const int preset) {
    session_settings.frame_delay_ms = presets[preset].frame_delay_ms;
    session_settings.run_ahead = coreinfo_feature_enabled(coreinfo_feature_run_ahead) ? presets[preset].run_ahead : 0;
    session_settings.gpu_hard_sync = presets[preset].gpu_hard_sync;
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_preset: {
            const int preset = current_preset();
            snprintf(
                buf, buf_len, "%s", preset < 0 ? lang.muxretro.settings_screen.preset_custom : presets[preset].name
            );
            break;
        }
        case row_fps_limit:
            snprintf(buf, buf_len, "%s", session_settings_fps_limit_name(session_settings.fps_limit));
            break;
        case row_frame_delay:
            snprintf(buf, buf_len, "%s", session_settings_frame_delay_name(session_settings.frame_delay_ms));
            break;
        case row_run_ahead:
            snprintf(buf, buf_len, "%s", session_settings.run_ahead ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_gpu_hard_sync:
            snprintf(buf, buf_len, "%s", session_settings.gpu_hard_sync ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_smoothness_check:
            switch (perf_check_smoothness()) {
                case perf_smoothness_collecting:
                    snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.smoothness_collecting);
                    break;
                case perf_smoothness_smooth:
                    snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.smoothness_smooth);
                    break;
                case perf_smoothness_uneven_frames:
                case perf_smoothness_audio_pressure:
                    snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.smoothness_review);
                    break;
                default:
                    snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.smoothness_not_run);
                    break;
            }
            break;
        case row_performance_capture:
            snprintf(buf, buf_len, "%s", perf_is_capture_active() ? lang.generic.enabled : lang.generic.disabled);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static submenu self;

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_preset: {
            const int preset = current_preset();
            apply_preset(preset < 0 ? preset_recommended : (preset + direction + preset_count) % preset_count);
            break;
        }
        case row_fps_limit:
            session_settings_cycle_fps_limit(direction);
            break;
        case row_frame_delay:
            session_settings_cycle_frame_delay(direction);
            break;
        case row_run_ahead:
            if (coreinfo_feature_enabled(coreinfo_feature_run_ahead)) session_settings_cycle_run_ahead(direction);
            break;
        case row_gpu_hard_sync:
            session_settings_cycle_gpu_hard_sync(direction);
            break;
        case row_performance_capture:
            advisor_capture_owned = 0;
            advisor_result_shown = 0;
            perf_set_capture_active(!perf_is_capture_active());
            break;
        default:
            break;
    }

    submenu_refresh_values(&self);
}

static int row_is_action(const int index) {
    return index == row_smoothness_check;
}

static int row_can_cycle(const int index) {
    return index != row_smoothness_check;
}

static const char *action_label(const int index) {
    return index == row_smoothness_check ? lang.generic.check : NULL;
}

static void row_action(const int index) {
    if (index != row_smoothness_check) return;

    if (advisor_result_shown) {
        perf_set_capture_active(1);
        advisor_capture_owned = 1;
        advisor_result_shown = 0;
        pause_menu_show_toast(lang.muxretro.settings_screen.smoothness_started);
        submenu_refresh_values(&self);
        return;
    }

    enum perf_smoothness_result result = perf_check_smoothness();
    if (result == perf_smoothness_not_run || result == perf_smoothness_collecting) {
        if (!perf_is_capture_active()) {
            perf_set_capture_active(1);
            advisor_capture_owned = 1;
        }
        pause_menu_show_toast(lang.muxretro.settings_screen.smoothness_started);
        submenu_refresh_values(&self);
        return;
    }

    if (advisor_capture_owned) {
        perf_set_capture_active(0);
        advisor_capture_owned = 0;
        advisor_result_shown = 1;
    }

    switch (result) {
        case perf_smoothness_audio_pressure:
            pause_menu_show_toast(lang.muxretro.settings_screen.smoothness_audio);
            break;
        case perf_smoothness_uneven_frames:
            pause_menu_show_toast(lang.muxretro.settings_screen.smoothness_frames);
            break;
        default:
            pause_menu_show_toast(lang.muxretro.settings_screen.smoothness_good);
            break;
    }
    submenu_refresh_values(&self);
}

static const char *extra_label(const int index) {
    return index == row_performance_capture ? lang.muxretro.settings_screen.export_diagnostics : NULL;
}

static void extra_action(const int index) {
    if (index != row_performance_capture) return;

    if (!perf_has_samples()) {
        pause_menu_show_toast(lang.muxretro.settings_screen.export_diagnostics_empty);
        return;
    }

    if (perf_export_trace(RETRO_SHARE_PATH "performance.csv") == 0) {
        pause_menu_show_toast(lang.muxretro.settings_screen.export_diagnostics_done);
    } else {
        pause_menu_show_toast(lang.muxretro.settings_screen.export_diagnostics_failed);
    }
}

static void closed(void) {
    settings_menu_reopen_performance();
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
    .action_label = action_label,
    .action = row_action,
    .extra_label = extra_label,
    .extra_action = extra_action,
    .closed = closed,
    .save_title = lang.muxretro.save.performance_title,
    .save_desc = lang.muxretro.save.performance_desc,
};

void performance_menu_init(void) {
    submenu_init(&self, &def);
}

void performance_menu_open(void) {
    submenu_open(&self);
}

int performance_menu_is_active(void) {
    return submenu_is_active(&self);
}

void performance_menu_tick(void) {
    submenu_tick(&self);
}
