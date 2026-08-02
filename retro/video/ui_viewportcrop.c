#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_crop_top = 0, row_crop_bottom, row_crop_left, row_crop_right, row_centre_crop, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.viewport_screen.crop_top, lang.muxretro.viewport_screen.crop_bottom,
    lang.muxretro.viewport_screen.crop_left, lang.muxretro.viewport_screen.crop_right,
    lang.muxretro.viewport_screen.centre_crop
};

static const char *row_glyphs[row_count] = {"croptop", "cropbottom", "cropleft", "cropright", "centrecrop"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
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

static int row_coarse_step(const int index) {
    return index == row_centre_crop ? 0 : 8;
}

static void closed(void) {
    viewport_menu_reopen_cropping();
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

void viewport_crop_menu_init(void) {
    submenu_init(&self, &def);
}

void viewport_crop_menu_open(void) {
    submenu_open(&self);
}

int viewport_crop_menu_is_active(void) {
    return submenu_is_active(&self);
}

void viewport_crop_menu_tick(void) {
    submenu_tick(&self);
}
