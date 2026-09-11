#include <stdio.h>
#include <string.h>
#include <common/platform/display.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../core/perf.h"
#include "../core/runahead.h"
#include "../coreinfo/coreinfo.h"
#include "../video/colour.h"
#include "../video/hw_render.h"
#include "../video/filters/filters.h"
#include "settings.h"
#include "pages.h"
#include "submenu.h"

enum {
    row_fps_limit = 0,
    row_frame_delay,
    row_run_ahead,
    row_gpu_hard_sync,
    row_state_thumbnail,
    row_performance_capture,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.fps_limit,       lang.muxretro.settings_screen.frame_delay,
    lang.muxretro.settings_screen.run_ahead,       lang.muxretro.settings_screen.gpu_hard_sync,
    lang.muxretro.settings_screen.state_thumbnail, lang.muxretro.settings_screen.performance_capture
};

static const char *row_glyphs[row_count] = {"fpslimit", "framedelay", "runahead", "hardsync", "state", "info"};

static const char *row_help[row_count] = {
    lang.muxretro.help.performance.fps_limit,       lang.muxretro.help.performance.frame_delay,
    lang.muxretro.help.performance.run_ahead,       lang.muxretro.help.performance.gpu_hard_sync,
    lang.muxretro.help.performance.state_thumbnail, lang.muxretro.help.performance.capture
};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_fps_limit:
            snprintf(buf, buf_len, "%s", session_settings_fps_limit_name(session_settings.fps_limit));
            break;
        case row_frame_delay:
            snprintf(buf, buf_len, "%s", session_settings_frame_delay_name(session_settings.frame_delay_ms));
            break;
        case row_run_ahead:
            if (!coreinfo_feature_enabled(coreinfo_feature_run_ahead) || hw_render_bridge_active())
                snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.diagnostic_unavailable);
            else
                snprintf(buf, buf_len, "%s", session_settings.run_ahead ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_gpu_hard_sync:
            snprintf(buf, buf_len, "%s", session_settings.gpu_hard_sync ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_state_thumbnail:
            snprintf(buf, buf_len, "%s", session_settings_state_thumbnail_name(session_settings.state_thumbnail));
            break;
        case row_performance_capture:
            snprintf(buf, buf_len, "%s", perf_is_capture_active() ? lang.generic.enabled : lang.generic.disabled);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static submenu diagnostics_self;

enum {
    diagnostic_refresh = 0,
    diagnostic_cadence,
    diagnostic_source,
    diagnostic_output,
    diagnostic_integer,
    diagnostic_filter,
    diagnostic_shader,
    diagnostic_shader_work,
    diagnostic_audio,
    diagnostic_corrections,
    diagnostic_run_ahead,
    diagnostic_stage,
    diagnostic_count
};

static const char *diagnostic_labels[diagnostic_count] = {
    lang.muxretro.settings_screen.diagnostic_refresh,         lang.muxretro.settings_screen.diagnostic_cadence,
    lang.muxretro.settings_screen.diagnostic_source,          lang.muxretro.settings_screen.diagnostic_output,
    lang.muxretro.settings_screen.diagnostic_integer_mapping, lang.muxretro.settings_screen.diagnostic_filter,
    lang.muxretro.settings_screen.diagnostic_shader,          lang.muxretro.settings_screen.diagnostic_shader_work,
    lang.muxretro.settings_screen.diagnostic_audio_queue,     lang.muxretro.settings_screen.diagnostic_corrections,
    lang.muxretro.settings_screen.diagnostic_run_ahead,       lang.muxretro.settings_screen.diagnostic_stage,
};

static const char *diagnostic_glyphs[diagnostic_count] = {
    "fpslimit", "framedelay", "scaling",      "scaling",    "integerscale", "texturefilter",
    "shader",   "shader",     "audiolatency", "shimmerfix", "runahead",     "info",
};

static void diagnostic_value_text(const int index, char *buf, const size_t buf_len) {
    int source_w = 0, source_h = 0, logical_w = 0, logical_h = 0, output_w = 0, output_h = 0, integer = 0;
    video_bridge_get_output_geometry(&source_w, &source_h, &logical_w, &logical_h, &output_w, &output_h, &integer);

    switch (index) {
        case diagnostic_refresh:
            snprintf(buf, buf_len, "%d / %.2f Hz", display_panel_refresh_hz(), frame_pacer_get_observed_hz());
            break;
        case diagnostic_cadence:
            if (perf_has_samples())
                snprintf(
                    buf, buf_len, "%.2f / %.2f ms", perf_stage_mean_ms(perf_stage_frame),
                    perf_stage_p95_ms(perf_stage_frame)
                );
            else
                snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.diagnostic_no_samples);
            break;
        case diagnostic_source:
            snprintf(buf, buf_len, "%dx%d", source_w, source_h);
            break;
        case diagnostic_output:
            snprintf(buf, buf_len, "%dx%d (logical %dx%d)", output_w, output_h, logical_w, logical_h);
            break;
        case diagnostic_integer: {
            const int scale =
                source_w > 0 && source_h > 0 && integer
                    ? ((output_w / source_w) < (output_h / source_h) ? (output_w / source_w) : (output_h / source_h))
                    : 0;
            if (integer)
                snprintf(buf, buf_len, "%s (%dx)", lang.generic.enabled, scale);
            else
                snprintf(buf, buf_len, "%s", lang.generic.disabled);
        } break;
        case diagnostic_filter:
            if (texture_filter_is_cpu_scaled(session_settings.texture_filter))
                snprintf(
                    buf, buf_len, "%s (%s)", session_settings_filter_name(session_settings.texture_filter),
                    video_bridge_cpu_filter_active() ? lang.generic.enabled
                                                     : lang.muxretro.settings_screen.diagnostic_bypassed
                );
            else
                snprintf(buf, buf_len, "%s", session_settings_filter_name(session_settings.texture_filter));
            break;
        case diagnostic_shader:
            snprintf(buf, buf_len, "%s", colour_shader_label(session_settings.colour_shader));
            break;
        case diagnostic_shader_work:
            snprintf(
                buf, buf_len, "%u ops / %.2f Mpix", colour_shader_render_operations(),
                (double) colour_shader_processed_pixels() / 1000000.0
            );
            break;
        case diagnostic_audio:
            snprintf(
                buf, buf_len, "%u ms (%u-%u)", audio_bridge_queued_ms(), audio_bridge_low_water_ms(),
                audio_bridge_high_water_ms()
            );
            break;
        case diagnostic_corrections:
            if (hw_render_bridge_active() || (session_settings.anti_flicker && !video_bridge_anti_flicker_available()))
                snprintf(
                    buf, buf_len, "%s: %s / %s: %s", lang.muxretro.settings_screen.shimmer_fix,
                    session_settings.shimmer_fix ? lang.generic.enabled : lang.generic.disabled,
                    lang.muxretro.settings_screen.anti_flicker, lang.muxretro.settings_screen.diagnostic_unavailable
                );
            else
                snprintf(
                    buf, buf_len, "%s: %s / %s: %s (%.1f MiB)", lang.muxretro.settings_screen.shimmer_fix,
                    session_settings.shimmer_fix ? lang.generic.enabled : lang.generic.disabled,
                    lang.muxretro.settings_screen.anti_flicker,
                    session_settings.anti_flicker ? lang.generic.enabled : lang.generic.disabled,
                    (double) video_bridge_anti_flicker_bytes() / (1024.0 * 1024.0)
                );
            break;
        case diagnostic_run_ahead:
            if (!session_settings.run_ahead)
                snprintf(buf, buf_len, "%s", lang.generic.disabled);
            else if (runahead_session_failed())
                snprintf(buf, buf_len, "%s", lang.muxretro.settings_screen.diagnostic_bypassed);
            else
                snprintf(
                    buf, buf_len, "%.1f KiB, capture %.2f ms, replay %.2f ms", (double) runahead_state_size() / 1024.0,
                    perf_stage_p95_ms(perf_stage_runahead_capture),
                    perf_stage_p95_ms(perf_stage_runahead_restore) + perf_stage_p95_ms(perf_stage_runahead_replay)
                );
            break;
        case diagnostic_stage:
            snprintf(
                buf, buf_len, "%s",
                perf_external_stage_active() ? lang.muxretro.settings_screen.diagnostic_bypassed
                                             : lang.muxretro.settings_screen.diagnostic_inactive
            );
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void diagnostics_closed(void) {
    settings_menu_reopen_video_diagnostics();
}

static const submenu_def diagnostics_def = {
    .labels = diagnostic_labels,
    .glyphs = diagnostic_glyphs,
    .row_count = diagnostic_count,
    .value_text = diagnostic_value_text,
    .closed = diagnostics_closed,
    .save_title = lang.muxretro.save.performance_title,
    .save_desc = lang.muxretro.save.performance_desc,
};

static void cycle_row(const int index, const int direction) {
    switch (index) {
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
        case row_state_thumbnail:
            session_settings_cycle_state_thumbnail(direction);
            break;
        case row_performance_capture:
            perf_set_capture_active(!perf_is_capture_active());
            break;
        default:
            break;
    }
}

static int row_can_cycle(const int index) {
    if (index == row_run_ahead)
        return coreinfo_feature_enabled(coreinfo_feature_run_ahead) && !hw_render_bridge_active();
    return 1;
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
    .extra_label = extra_label,
    .extra_action = extra_action,
    .closed = closed,
    .save_title = lang.muxretro.save.performance_title,
    .save_desc = lang.muxretro.save.performance_desc,
};

void performance_menu_init(void) {
    submenu_init(&diagnostics_self, &diagnostics_def);
}

void video_diagnostics_menu_open(void) {
    submenu_open(&diagnostics_self);
}

int video_diagnostics_menu_is_active(void) {
    return submenu_is_active(&diagnostics_self);
}

void video_diagnostics_menu_tick(void) {
    static uint32_t refresh_at;
    const uint32_t now = SDL_GetTicks();
    if (SDL_TICKS_PASSED(now, refresh_at)) {
        submenu_refresh_values(&diagnostics_self);
        refresh_at = now + 250;
    }
    submenu_tick(&diagnostics_self);
}

const submenu_def *performance_menu_definition(void) {
    return &def;
}
