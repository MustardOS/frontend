#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_viewport = 0,
    row_scaling,
    row_rotate,
    row_mirrored,
    row_aspect_ratio,
    row_integer_scale,
    row_filter,
    row_shimmer_fix,
    row_border,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.display_screen.viewport,
    lang.muxretro.settings_screen.scaling_mode,
    lang.muxretro.settings_screen.rotate,
    lang.muxretro.settings_screen.mirrored,
    lang.muxretro.settings_screen.aspect_ratio_mode,
    lang.muxretro.settings_screen.integer_scale,
    lang.muxretro.settings_screen.texture_filter,
    lang.muxretro.settings_screen.shimmer_fix,
    lang.muxretro.settings_screen.border_colour
};

static const char *row_glyphs[row_count] = {"viewport",     "scaling",       "rotate",     "mirrored", "aspectratio",
                                            "integerscale", "texturefilter", "shimmerfix", "border"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_scaling:
            snprintf(buf, buf_len, "%s", session_settings_scale_name(session_settings.scaling_mode));
            break;
        case row_rotate:
            snprintf(buf, buf_len, "%s", session_settings_rotate_name(session_settings.rotate));
            break;
        case row_mirrored:
            snprintf(buf, buf_len, "%s", session_settings.mirrored ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_aspect_ratio:
            snprintf(buf, buf_len, "%s", session_settings_aspect_ratio_name(session_settings.aspect_ratio));
            break;
        case row_integer_scale:
            snprintf(buf, buf_len, "%s", session_settings_integer_scale_name(session_settings.integer_scale));
            break;
        case row_filter:
            snprintf(buf, buf_len, "%s", session_settings_filter_name(session_settings.texture_filter));
            break;
        case row_shimmer_fix:
            snprintf(buf, buf_len, "%s", session_settings.shimmer_fix ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_border:
            snprintf(buf, buf_len, "%s", session_settings_border_name(session_settings.border_color));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_scaling:
            session_settings_cycle_scaling(direction);
            break;
        case row_rotate:
            session_settings_cycle_rotate(direction);
            break;
        case row_mirrored:
            session_settings_cycle_mirrored(direction);
            break;
        case row_aspect_ratio:
            session_settings_cycle_aspect_ratio(direction);
            break;
        case row_integer_scale:
            session_settings_cycle_integer_scale(direction);
            break;
        case row_filter:
            session_settings_cycle_filter(direction);
            break;
        case row_shimmer_fix:
            session_settings_cycle_shimmer_fix(direction);
            break;
        case row_border:
            session_settings_cycle_border(direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    return index == row_viewport;
}

static void row_action(const int index) {
    if (index == row_viewport) viewport_menu_open();
}

static int child_tick(void) {
    if (viewport_menu_is_active()) {
        viewport_menu_tick();
        return 1;
    }
    return 0;
}

static void closed(void) {
    settings_menu_reopen_video();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.video_title,
    .save_desc = lang.muxretro.save.video_desc,
};

void video_menu_init(void) {
    submenu_init(&self, &def);
    viewport_menu_init();
}

void video_menu_open(void) {
    submenu_open(&self);
}

int video_menu_is_active(void) {
    return submenu_is_active(&self);
}

void video_menu_tick(void) {
    submenu_tick(&self);
}

void video_menu_reopen_viewport(void) {
    submenu_reopen_at(&self, row_viewport);
}
