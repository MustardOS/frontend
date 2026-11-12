#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "settings.h"
#include "pages.h"
#include "submenu.h"

enum {
    row_content_clock = 0,
    row_clock_year,
    row_clock_month,
    row_clock_day,
    row_clock_hour,
    row_clock_minute,
    row_count
};

static submenu self;

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.content_clock, lang.muxretro.settings_screen.clock_year,
    lang.muxretro.settings_screen.clock_month,   lang.muxretro.settings_screen.clock_day,
    lang.muxretro.settings_screen.clock_hour,    lang.muxretro.settings_screen.clock_minute
};

static const char *row_glyphs[row_count] = {"clock", "year", "month", "day", "hour", "minute"};

static const char *row_help[row_count] = {
    lang.muxretro.help.performance.content_clock, lang.muxretro.help.performance.clock_field,
    lang.muxretro.help.performance.clock_field,   lang.muxretro.help.performance.clock_field,
    lang.muxretro.help.performance.clock_field,   lang.muxretro.help.performance.clock_field
};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_content_clock:
            snprintf(
                buf, buf_len, "%s",
                session_settings.content_clock ? lang.muxretro.settings_screen.content_clock_custom
                                               : lang.muxretro.settings_screen.content_clock_device
            );
            break;
        case row_clock_year:
            snprintf(buf, buf_len, "%d", session_settings.clock_year);
            break;
        case row_clock_month:
            snprintf(buf, buf_len, "%02d", session_settings.clock_month);
            break;
        case row_clock_day:
            snprintf(buf, buf_len, "%02d", session_settings.clock_day);
            break;
        case row_clock_hour:
            snprintf(buf, buf_len, "%02d", session_settings.clock_hour);
            break;
        case row_clock_minute:
            snprintf(buf, buf_len, "%02d", session_settings.clock_minute);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_content_clock:
            session_settings_cycle_content_clock(direction);
            break;
        case row_clock_year:
            session_settings_cycle_clock_field(&session_settings.clock_year, 1980, 2099, direction);
            break;
        case row_clock_month:
            session_settings_cycle_clock_field(&session_settings.clock_month, 1, 12, direction);
            break;
        case row_clock_day:
            session_settings_cycle_clock_field(&session_settings.clock_day, 1, 31, direction);
            break;
        case row_clock_hour:
            session_settings_cycle_clock_field(&session_settings.clock_hour, 0, 23, direction);
            break;
        case row_clock_minute:
            session_settings_cycle_clock_field(&session_settings.clock_minute, 0, 59, direction);
            break;
        default:
            break;
    }
}

static int row_can_cycle(const int index) {
    return index == row_content_clock || session_settings.content_clock;
}

static void closed(void) {
    settings_menu_reopen_content_clock();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_can_cycle = row_can_cycle,
    .closed = closed,
    .save_title = lang.muxretro.save.performance_title,
    .save_desc = lang.muxretro.save.performance_desc,
};

void content_clock_menu_init(void) {
    submenu_init(&self, &def);
}

void content_clock_menu_open(void) {
    submenu_open(&self);
}

int content_clock_menu_is_active(void) {
    return submenu_is_active(&self);
}

void content_clock_menu_tick(void) {
    submenu_tick(&self);
}
