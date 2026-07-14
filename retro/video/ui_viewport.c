#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_offset_x = 0,
    row_offset_y,
    row_zoom,
    row_crop_top,
    row_crop_bottom,
    row_crop_left,
    row_crop_right,
    row_centre_crop,
    row_reset,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.viewport_screen.offset_x,    lang.muxretro.viewport_screen.offset_y,
    lang.muxretro.viewport_screen.zoom,        lang.muxretro.viewport_screen.crop_top,
    lang.muxretro.viewport_screen.crop_bottom, lang.muxretro.viewport_screen.crop_left,
    lang.muxretro.viewport_screen.crop_right,  lang.muxretro.viewport_screen.centre_crop,
    lang.muxretro.viewport_screen.reset
};

static const char *row_glyphs[row_count] = {"viewportx", "viewporty", "viewportzoom", "croptop",      "cropbottom",
                                            "cropleft",  "cropright", "centrecrop",   "viewportreset"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_offset_x:
            snprintf(buf, buf_len, "%s", session_settings_viewport_offset_x_name(session_settings.viewport_offset_x));
            break;
        case row_offset_y:
            snprintf(buf, buf_len, "%s", session_settings_viewport_offset_y_name(session_settings.viewport_offset_y));
            break;
        case row_zoom:
            snprintf(buf, buf_len, "%s", session_settings_viewport_zoom_name(session_settings.viewport_zoom));
            break;
        case row_crop_top:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.viewport_crop_top));
            break;
        case row_crop_bottom:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.viewport_crop_bottom));
            break;
        case row_crop_left:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.viewport_crop_left));
            break;
        case row_crop_right:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.viewport_crop_right));
            break;
        case row_centre_crop:
            snprintf(
                buf, buf_len, "%s", session_settings.viewport_centre_crop ? lang.generic.enabled : lang.generic.disabled
            );
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
        case row_zoom:
            session_settings_cycle_viewport_zoom(direction);
            break;
        case row_crop_top:
            session_settings_cycle_viewport_crop_top(direction);
            break;
        case row_crop_bottom:
            session_settings_cycle_viewport_crop_bottom(direction);
            break;
        case row_crop_left:
            session_settings_cycle_viewport_crop_left(direction);
            break;
        case row_crop_right:
            session_settings_cycle_viewport_crop_right(direction);
            break;
        case row_centre_crop:
            session_settings_cycle_viewport_centre_crop(direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    return index == row_reset;
}

static submenu self;

static void row_action(const int index) {
    if (index == row_reset) {
        session_settings_reset_viewport();
        submenu_refresh_values(&self);
    }
}

static void closed(void) {
    video_menu_reopen_viewport();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action = row_action,
    .closed = closed,
    .save_title = lang.muxretro.save.viewport_title,
    .save_desc = lang.muxretro.save.viewport_desc,
};

void viewport_menu_init(void) {
    submenu_init(&self, &def);
}

void viewport_menu_open(void) {
    submenu_open(&self);
}

int viewport_menu_is_active(void) {
    return submenu_is_active(&self);
}

void viewport_menu_tick(void) {
    submenu_tick(&self);
}
