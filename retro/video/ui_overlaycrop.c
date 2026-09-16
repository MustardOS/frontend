#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_crop_top = 0, row_crop_bottom, row_crop_left, row_crop_right, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.viewport_screen.crop_top, lang.muxretro.viewport_screen.crop_bottom,
    lang.muxretro.viewport_screen.crop_left, lang.muxretro.viewport_screen.crop_right
};

static const char *row_glyphs[row_count] = {"croptop", "cropbottom", "cropleft", "cropright"};

static const char *row_help[row_count] = {
    lang.muxretro.help.viewport.crop_top, lang.muxretro.help.viewport.crop_bottom,
    lang.muxretro.help.viewport.crop_left, lang.muxretro.help.viewport.crop_right
};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_crop_top:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.overlay_crop_top));
            break;
        case row_crop_bottom:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.overlay_crop_bottom));
            break;
        case row_crop_left:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.overlay_crop_left));
            break;
        case row_crop_right:
            snprintf(buf, buf_len, "%s", session_settings_viewport_crop_name(session_settings.overlay_crop_right));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_crop_top:
            session_settings_cycle_overlay_crop_top(direction);
            break;
        case row_crop_bottom:
            session_settings_cycle_overlay_crop_bottom(direction);
            break;
        case row_crop_left:
            session_settings_cycle_overlay_crop_left(direction);
            break;
        case row_crop_right:
            session_settings_cycle_overlay_crop_right(direction);
            break;
        default:
            break;
    }
}

static int row_coarse_step(const int index) {
    (void) index;
    return 16;
}

static void closed(void) {
    settings_menu_reopen_overlay_at(overlay_menu_row_cropping());
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_coarse_step = row_coarse_step,
    .closed = closed,
    .save_title = lang.muxretro.save.display_title,
    .save_desc = lang.muxretro.save.display_desc,
};

void overlay_crop_menu_init(void) {
    submenu_init(&self, &def);
}

void overlay_crop_menu_open(void) {
    submenu_open(&self);
}

int overlay_crop_menu_is_active(void) {
    return submenu_is_active(&self);
}

void overlay_crop_menu_tick(void) {
    submenu_tick(&self);
}
