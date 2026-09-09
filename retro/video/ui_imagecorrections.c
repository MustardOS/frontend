#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"
#include "hw_render.h"

enum { row_shimmer_fix = 0, row_anti_flicker, row_max };

static const char *all_labels[row_max] = {
    lang.muxretro.settings_screen.shimmer_fix, lang.muxretro.settings_screen.anti_flicker
};

static const char *all_glyphs[row_max] = {"shimmerfix", "shimmerfix"};

static const char *all_help[row_max] = {lang.muxretro.help.video.shimmer_fix, lang.muxretro.help.video.anti_flicker};

static const char *row_labels[row_max];
static const char *row_glyphs[row_max];
static const char *row_help[row_max];
static int row_map[row_max];
static int row_total;

static void build_rows(void) {
    row_total = 0;

    for (int i = 0; i < row_max; i++) {
        if (i == row_anti_flicker && hw_render_bridge_active()) continue;

        row_labels[row_total] = all_labels[i];
        row_glyphs[row_total] = all_glyphs[i];
        row_help[row_total] = all_help[i];
        row_map[row_total] = i;
        row_total++;
    }
}

static void row_value_text(const int display_index, char *buf, const size_t buf_len) {
    switch (row_map[display_index]) {
        case row_shimmer_fix:
            snprintf(buf, buf_len, "%s", session_settings.shimmer_fix ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_anti_flicker:
            snprintf(buf, buf_len, "%s", session_settings.anti_flicker ? lang.generic.enabled : lang.generic.disabled);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int display_index, const int direction) {
    switch (row_map[display_index]) {
        case row_shimmer_fix:
            session_settings_cycle_shimmer_fix(direction);
            break;
        case row_anti_flicker:
            session_settings_cycle_anti_flicker(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    video_menu_reopen_image_corrections();
}

static submenu self;

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

void image_corrections_menu_init(void) {
    build_rows();
    def.row_count = row_total;
    submenu_init(&self, &def);
}

void image_corrections_menu_open(void) {
    build_rows();
    def.row_count = row_total;
    submenu_open(&self);
}

int image_corrections_menu_is_active(void) {
    return submenu_is_active(&self);
}

void image_corrections_menu_tick(void) {
    submenu_tick(&self);
}

int image_corrections_settings_child_tick(void) {
    if (!image_corrections_menu_is_active()) return 0;
    image_corrections_menu_tick();
    return 1;
}
