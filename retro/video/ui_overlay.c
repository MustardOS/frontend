#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/pages.h"
#include "../settings/submenu.h"

enum {
    logical_source = 0,
    logical_pattern,
    logical_image,
    logical_opacity,
    logical_adjustment,
    logical_cropping,
    logical_reset,
    logical_count
};

static const char *logical_labels[logical_count] = {
    lang.muxretro.display_screen.overlay,         lang.muxretro.display_screen.overlay_pattern,
    lang.muxretro.overlay_screen.image,           lang.muxretro.display_screen.overlay_opacity,
    lang.muxretro.overlay_screen.adjustment,      lang.muxretro.overlay_screen.cropping,
    lang.muxretro.overlay_screen.reset
};

static const char *logical_glyphs[logical_count] = {"overlay",  "overlaypattern", "overlay",       "overlayopacity",
                                                    "viewport", "centrecrop",     "viewportreset"};

static const char *logical_help[logical_count] = {
    lang.muxretro.help.overlay.source,     lang.muxretro.help.overlay.pattern,
    lang.muxretro.help.overlay.image,      lang.muxretro.help.overlay.opacity,
    lang.muxretro.help.overlay.adjustment, lang.muxretro.help.overlay.cropping,
    lang.muxretro.help.overlay.reset
};

static int visible_rows[logical_count];
static const char *row_labels[logical_count];
static const char *row_glyphs[logical_count];
static const char *row_help[logical_count];
static int visible_count;
static int rows_dirty = 0;

static int row_is_visible(const int logical) {
    const int showing = session_settings.overlay_source != overlay_source_off;

    switch (logical) {
        case logical_pattern: return session_settings.overlay_source == overlay_source_pattern;
        case logical_image: return session_settings.overlay_source == overlay_source_downloaded;
        case logical_opacity:
        case logical_adjustment:
        case logical_cropping:
        case logical_reset: return showing;
        default: return 1;
    }
}

static void cycle_row(int index, int direction);

static void row_value_text(int index, char *buf, size_t buf_len);

static void closed(void);

static int row_is_action(int index);

static void row_action(int index);

static submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = logical_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action = row_action,
    .closed = closed,
    .save_title = lang.muxretro.save.display_title,
    .save_desc = lang.muxretro.save.display_desc,
};

static void configure_rows(void) {
    visible_count = 0;

    for (int logical = 0; logical < logical_count; logical++) {
        if (!row_is_visible(logical)) continue;

        visible_rows[visible_count] = logical;
        row_labels[visible_count] = logical_labels[logical];
        row_glyphs[visible_count] = logical_glyphs[logical];
        row_help[visible_count] = logical_help[logical];
        visible_count++;
    }

    def.row_count = visible_count;
}

static int logical_of(const int index) {
    return index >= 0 && index < visible_count ? visible_rows[index] : -1;
}

static int row_of_logical(const int logical) {
    for (int index = 0; index < visible_count; index++)
        if (visible_rows[index] == logical) return index;

    return 0;
}

int overlay_menu_row_image(void) {
    return row_of_logical(logical_image);
}

int overlay_menu_row_adjustment(void) {
    return row_of_logical(logical_adjustment);
}

int overlay_menu_row_cropping(void) {
    return row_of_logical(logical_cropping);
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (logical_of(index)) {
        case logical_source:
            snprintf(buf, buf_len, "%s", session_settings_overlay_source_name(session_settings.overlay_source));
            break;
        case logical_pattern:
            snprintf(buf, buf_len, "%s", session_settings_overlay_pattern_name(session_settings.overlay_pattern));
            break;
        case logical_image:
            snprintf(buf, buf_len, "%s", session_settings_overlay_image_name(session_settings.overlay_image));
            break;
        case logical_opacity:
            snprintf(buf, buf_len, "%s", session_settings_overlay_opacity_name(session_settings.overlay_opacity));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static submenu self;

static void cycle_row(const int index, const int direction) {
    switch (logical_of(index)) {
        case logical_source:
            session_settings_cycle_overlay_source(direction);

            rows_dirty = 1;
            break;
        case logical_pattern:
            session_settings_cycle_overlay_pattern(direction);
            break;
        case logical_opacity:
            session_settings_cycle_overlay_opacity(direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    switch (logical_of(index)) {
        case logical_image:
        case logical_adjustment:
        case logical_cropping:
        case logical_reset: return 1;
        default: return 0;
    }
}

static void row_action(const int index) {
    switch (logical_of(index)) {
        case logical_image:
            overlay_image_menu_open();
            break;
        case logical_adjustment:
            overlay_adjust_menu_open();
            break;
        case logical_cropping:
            overlay_crop_menu_open();
            break;
        case logical_reset:
            session_settings_reset_overlay();
            break;
        default:
            break;
    }
}

int overlay_settings_child_tick(void) {
    if (rows_dirty) {
        rows_dirty = 0;
        configure_rows();
        settings_menu_reopen_overlay();
        return 1;
    }

    if (overlay_image_menu_is_active()) {
        overlay_image_menu_tick();
        return 1;
    }

    if (overlay_adjust_menu_is_active()) {
        overlay_adjust_menu_tick();
        return 1;
    }

    if (overlay_crop_menu_is_active()) {
        overlay_crop_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    settings_menu_reopen_overlay();
}

void overlay_menu_init(void) {
    configure_rows();
    submenu_init(&self, &def);

    overlay_image_menu_init();
    overlay_adjust_menu_init();
    overlay_crop_menu_init();
}

void overlay_menu_open(void) {
    configure_rows();
    submenu_open(&self);
}

int overlay_menu_is_active(void) {
    return submenu_is_active(&self);
}

void overlay_menu_tick(void) {
    submenu_tick(&self);
}

const submenu_def *overlay_menu_definition(void) {
    configure_rows();
    return &def;
}
