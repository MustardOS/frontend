#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "settings.h"
#include "submenu.h"

enum { row_fps_limit = 0, row_frame_delay, row_run_ahead, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.fps_limit, lang.muxretro.settings_screen.frame_delay,
    lang.muxretro.settings_screen.run_ahead
};

static const char *row_glyphs[row_count] = {"fpslimit", "framedelay", "runahead"};

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
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_fps_limit:
            session_settings_cycle_fps_limit(direction);
            break;
        case row_frame_delay:
            session_settings_cycle_frame_delay(direction);
            break;
        case row_run_ahead:
            session_settings_cycle_run_ahead(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_performance();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
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
