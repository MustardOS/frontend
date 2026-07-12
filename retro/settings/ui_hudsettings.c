#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "settings.h"
#include "submenu.h"

enum { row_fps = 0, row_header_visibility, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.show_fps, lang.muxretro.settings_screen.header_visibility
};

static const char *row_glyphs[row_count] = {"fpscounter", "header"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_fps:
            snprintf(buf, buf_len, "%s", session_settings.show_fps ? lang.generic.enabled : lang.generic.disabled);
            break;
        case row_header_visibility:
            snprintf(buf, buf_len, "%s", session_settings_header_visibility_name(session_settings.header_visibility));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_fps:
            session_settings_cycle_fps(direction);
            break;
        case row_header_visibility:
            session_settings_cycle_header_visibility(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_hud();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.hud_title,
    .save_desc = lang.muxretro.save.hud_desc,
};

void hud_menu_init(void) {
    submenu_init(&self, &def);
}

void hud_menu_open(void) {
    submenu_open(&self);
}

int hud_menu_is_active(void) {
    return submenu_is_active(&self);
}

void hud_menu_tick(void) {
    submenu_tick(&self);
}
