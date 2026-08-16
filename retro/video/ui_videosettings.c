#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/pages.h"
#include "../settings/submenu.h"

enum {
    row_scaling = 0,
    row_rotate,
    row_mirrored,
    row_aspect_ratio,
    row_integer_scale,
    row_filter,
    row_shimmer_fix,
    row_border,
    row_game_renderer,
    row_max
};

static const char *all_labels[row_max] = {
    lang.muxretro.settings_screen.scaling_mode,  lang.muxretro.settings_screen.rotate,
    lang.muxretro.settings_screen.mirrored,      lang.muxretro.settings_screen.aspect_ratio_mode,
    lang.muxretro.settings_screen.integer_scale, lang.muxretro.settings_screen.texture_filter,
    lang.muxretro.settings_screen.shimmer_fix,   lang.muxretro.settings_screen.border_colour,
    lang.muxretro.settings_screen.game_renderer
};

static const char *all_glyphs[row_max] = {"scaling",       "rotate",     "mirrored", "aspectratio", "integerscale",
                                          "texturefilter", "shimmerfix", "border",   "gamerenderer"};

static const char *all_help[row_max] = {lang.muxretro.help.video.scaling,       lang.muxretro.help.video.rotate,
                                        lang.muxretro.help.video.mirrored,      lang.muxretro.help.video.aspect_ratio,
                                        lang.muxretro.help.video.integer_scale, lang.muxretro.help.video.texture_filter,
                                        lang.muxretro.help.video.shimmer_fix,   lang.muxretro.help.video.border,
                                        lang.muxretro.help.video.game_renderer};

// The renderer choice only means anything to a core that asked for hardware rendering!
static const char *row_labels[row_max];
static const char *row_glyphs[row_max];
static const char *row_help[row_max];
static int row_map[row_max];
static int row_total;

static void build_rows(void) {
    row_total = 0;

    for (int i = 0; i < row_max; i++) {
        if (i == row_game_renderer && !environment_core_wants_hw_render()) continue;

        row_labels[row_total] = all_labels[i];
        row_glyphs[row_total] = all_glyphs[i];
        row_help[row_total] = all_help[i];
        row_map[row_total] = i;
        row_total++;
    }
}

static void row_value_text(const int display_index, char *buf, const size_t buf_len) {
    const int index = row_map[display_index];

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
            snprintf(buf, buf_len, "%s", session_settings_border_name(session_settings.border_colour));
            break;
        case row_game_renderer:
            snprintf(buf, buf_len, "%s", session_settings_game_renderer_name(session_settings.game_renderer));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int display_index, const int direction) {
    const int index = row_map[display_index];

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
        case row_game_renderer:
            session_settings_cycle_game_renderer(direction);
            pause_menu_show_toast(lang.muxretro.settings_screen.game_renderer_restart);
            break;
        default:
            break;
    }
}

static submenu self;

static void closed(void) {
    settings_menu_reopen_video();
}

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_max,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.video_title,
    .save_desc = lang.muxretro.save.video_desc,
};

void video_menu_init(void) {
    build_rows();
    def.row_count = row_total;

    submenu_init(&self, &def);
}

void video_menu_open(void) {
    build_rows();
    def.row_count = row_total;

    submenu_open(&self);
}

int video_menu_is_active(void) {
    return submenu_is_active(&self);
}

void video_menu_tick(void) {
    submenu_tick(&self);
}

const submenu_def *video_menu_definition(void) {
    build_rows();
    def.row_count = row_total;
    return &def;
}
