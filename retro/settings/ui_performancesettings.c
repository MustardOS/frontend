#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../core/perf.h"
#include "../coreinfo/coreinfo.h"
#include "settings.h"
#include "pages.h"
#include "submenu.h"

enum { row_fps_limit = 0, row_frame_delay, row_run_ahead, row_gpu_hard_sync, row_performance_capture, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.fps_limit, lang.muxretro.settings_screen.frame_delay,
    lang.muxretro.settings_screen.run_ahead, lang.muxretro.settings_screen.gpu_hard_sync,
    lang.muxretro.settings_screen.performance_capture
};

static const char *row_glyphs[row_count] = {"fpslimit", "framedelay", "runahead", "hardsync", "info"};

static const char *row_help[row_count] = {
    lang.muxretro.help.performance.fps_limit, lang.muxretro.help.performance.frame_delay,
    lang.muxretro.help.performance.run_ahead, lang.muxretro.help.performance.gpu_hard_sync,
    lang.muxretro.help.performance.capture
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
            snprintf(buf, buf_len, "%s", session_settings.run_ahead ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_gpu_hard_sync:
            snprintf(buf, buf_len, "%s", session_settings.gpu_hard_sync ? lang.generic.enabled : lang.generic.disabled);
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
            perf_set_capture_active(!perf_is_capture_active());
            break;
        default:
            break;
    }
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

const submenu_def *performance_menu_definition(void) {
    return &def;
}
