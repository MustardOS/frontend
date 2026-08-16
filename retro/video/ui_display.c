#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/pages.h"
#include "../settings/submenu.h"

enum { row_filter = 0, row_shader, row_brightness, row_contrast, row_saturation, row_hue_shift, row_gamma, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.display_screen.filter,     lang.muxretro.display_screen.shaders,
    lang.muxretro.display_screen.brightness, lang.muxretro.display_screen.contrast,
    lang.muxretro.display_screen.saturation, lang.muxretro.display_screen.hue_shift,
    lang.muxretro.display_screen.gamma
};

static const char *row_glyphs[row_count] = {"filter", "shader", "brightness", "contrast", "saturation", "hue", "gamma"};

static const char *row_help[row_count] = {lang.muxretro.help.display.filter,     lang.muxretro.help.display.shader,
                                          lang.muxretro.help.display.brightness, lang.muxretro.help.display.contrast,
                                          lang.muxretro.help.display.saturation, lang.muxretro.help.display.hue_shift,
                                          lang.muxretro.help.display.gamma};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_filter:
            snprintf(buf, buf_len, "%s", session_settings_colour_filter_name(session_settings.colour_filter));
            break;
        case row_shader:
            snprintf(buf, buf_len, "%s", session_settings_colour_shader_name(session_settings.colour_shader));
            break;
        case row_brightness:
            snprintf(buf, buf_len, "%s", session_settings_colour_brightness_name(session_settings.colour_brightness));
            break;
        case row_contrast:
            snprintf(buf, buf_len, "%s", session_settings_colour_contrast_name(session_settings.colour_contrast));
            break;
        case row_saturation:
            snprintf(buf, buf_len, "%s", session_settings_colour_saturation_name(session_settings.colour_saturation));
            break;
        case row_hue_shift:
            snprintf(buf, buf_len, "%s", session_settings_colour_hueshift_name(session_settings.colour_hueshift));
            break;
        case row_gamma:
            snprintf(buf, buf_len, "%s", session_settings_colour_gamma_name(session_settings.colour_gamma));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_brightness:
            session_settings_cycle_colour_brightness(direction);
            break;
        case row_contrast:
            session_settings_cycle_colour_contrast(direction);
            break;
        case row_saturation:
            session_settings_cycle_colour_saturation(direction);
            break;
        case row_hue_shift:
            session_settings_cycle_colour_hueshift(direction);
            break;
        case row_gamma:
            session_settings_cycle_colour_gamma(direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    return index == row_filter || index == row_shader;
}

static void row_action(const int index) {
    if (index == row_filter) {
        colfilter_menu_open();
    } else if (index == row_shader) {
        shader_menu_open();
    }
}

static int child_tick(void) {
    if (colfilter_menu_is_active()) {
        colfilter_menu_tick();
        return 1;
    }

    if (shader_menu_is_active()) {
        shader_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    settings_menu_reopen_visuals();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_is_action = row_is_action,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.display_title,
    .save_desc = lang.muxretro.save.display_desc,
};

void display_menu_init(void) {
    submenu_init(&self, &def);

    colfilter_menu_init();
    shader_menu_init();
}

void display_menu_open(void) {
    submenu_open(&self);
}

int display_menu_is_active(void) {
    return submenu_is_active(&self);
}

void display_menu_tick(void) {
    submenu_tick(&self);
}

void display_menu_reopen_filter(void) {
    settings_menu_reopen_visuals_at(row_filter);
}

void display_menu_reopen_shader(void) {
    settings_menu_reopen_visuals_at(row_shader);
}

const submenu_def *visuals_menu_definition(void) {
    return &def;
}
