#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_offset_x = 0, row_offset_y, row_stretch_x, row_stretch_y, row_zoom, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.viewport_screen.offset_x, lang.muxretro.viewport_screen.offset_y,
    lang.muxretro.viewport_screen.stretch_x, lang.muxretro.viewport_screen.stretch_y, lang.muxretro.viewport_screen.zoom
};

static const char *row_glyphs[row_count] = {"viewportx", "viewporty", "viewportx", "viewporty", "viewportzoom"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_offset_x:
            snprintf(buf, buf_len, "%s", session_settings_viewport_offset_x_name(session_settings.viewport_offset_x));
            break;
        case row_offset_y:
            snprintf(buf, buf_len, "%s", session_settings_viewport_offset_y_name(session_settings.viewport_offset_y));
            break;
        case row_stretch_x:
            snprintf(buf, buf_len, "%s", session_settings_viewport_stretch_name(session_settings.viewport_stretch_x));
            break;
        case row_stretch_y:
            snprintf(buf, buf_len, "%s", session_settings_viewport_stretch_name(session_settings.viewport_stretch_y));
            break;
        case row_zoom:
            snprintf(buf, buf_len, "%s", session_settings_viewport_zoom_name(session_settings.viewport_zoom));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_offset_x:
            session_settings_cycle_viewport_offset_x(direction);
            break;
        case row_offset_y:
            session_settings_cycle_viewport_offset_y(direction);
            break;
        case row_stretch_x:
            session_settings_cycle_viewport_stretch_x(direction);
            break;
        case row_stretch_y:
            session_settings_cycle_viewport_stretch_y(direction);
            break;
        case row_zoom:
            session_settings_cycle_viewport_zoom(direction);
            break;
        default:
            break;
    }
}

static int row_coarse_step(const int index) {
    return index == row_zoom ? 0 : 16;
}

static void closed(void) {
    viewport_menu_reopen_adjustment();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_coarse_step = row_coarse_step,
    .closed = closed,
    .save_title = lang.muxretro.save.viewport_title,
    .save_desc = lang.muxretro.save.viewport_desc,
};

void viewport_adjust_menu_init(void) {
    submenu_init(&self, &def);
}

void viewport_adjust_menu_open(void) {
    submenu_open(&self);
}

int viewport_adjust_menu_is_active(void) {
    return submenu_is_active(&self);
}

void viewport_adjust_menu_tick(void) {
    submenu_tick(&self);
}
