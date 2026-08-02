#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_overlay_source = 0, row_overlay_pattern, row_overlay_opacity, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.display_screen.overlay, lang.muxretro.display_screen.overlay_pattern,
    lang.muxretro.display_screen.overlay_opacity
};

static const char *row_glyphs[row_count] = {"overlay", "overlaypattern", "overlayopacity"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_overlay_source:
            snprintf(buf, buf_len, "%s", session_settings_overlay_source_name(session_settings.overlay_source));
            break;
        case row_overlay_pattern:
            snprintf(buf, buf_len, "%s", session_settings_overlay_pattern_name(session_settings.overlay_pattern));
            break;
        case row_overlay_opacity:
            snprintf(buf, buf_len, "%s", session_settings_overlay_opacity_name(session_settings.overlay_opacity));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_overlay_source:
            session_settings_cycle_overlay_source(direction);
            break;
        case row_overlay_pattern:
            session_settings_cycle_overlay_pattern(direction);
            break;
        case row_overlay_opacity:
            session_settings_cycle_overlay_opacity(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    video_menu_reopen_overlay();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.display_title,
    .save_desc = lang.muxretro.save.display_desc,
};

void overlay_menu_init(void) {
    submenu_init(&self, &def);
}

void overlay_menu_open(void) {
    submenu_open(&self);
}

int overlay_menu_is_active(void) {
    return submenu_is_active(&self);
}

void overlay_menu_tick(void) {
    submenu_tick(&self);
}
