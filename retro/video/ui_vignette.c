#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum {
    row_shape = 0,
    row_scaling,
    row_width,
    row_height,
    row_offset_x,
    row_offset_y,
    row_softness,
    row_strength,
    row_colour,
    row_count
};

static const char *row_labels[row_count] = {
    lang.muxretro.vignette_screen.shape,    lang.muxretro.vignette_screen.scaling,
    lang.muxretro.vignette_screen.width,    lang.muxretro.vignette_screen.height,
    lang.muxretro.vignette_screen.offset_x, lang.muxretro.vignette_screen.offset_y,
    lang.muxretro.vignette_screen.softness, lang.muxretro.vignette_screen.strength,
    lang.muxretro.vignette_screen.colour
};

static const char *row_glyphs[row_count] = {"border",    "aspectratio",   "viewportx", "viewporty", "viewportx",
                                            "viewporty", "texturefilter", "contrast",  "saturation"};

static const char *row_help[row_count] = {lang.muxretro.help.vignette.shape,    lang.muxretro.help.vignette.scaling,
                                          lang.muxretro.help.vignette.width,    lang.muxretro.help.vignette.height,
                                          lang.muxretro.help.vignette.offset_x, lang.muxretro.help.vignette.offset_y,
                                          lang.muxretro.help.vignette.softness, lang.muxretro.help.vignette.strength,
                                          lang.muxretro.help.vignette.colour};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_shape:
            snprintf(buf, buf_len, "%s", session_settings_vignette_shape_name(session_settings.vignette_shape));
            break;
        case row_scaling:
            snprintf(buf, buf_len, "%s", session_settings_vignette_scaling_name(session_settings.vignette_scaling));
            break;
        case row_width:
            snprintf(buf, buf_len, "%s", session_settings_vignette_size_name(session_settings.vignette_width));
            break;
        case row_height:
            snprintf(buf, buf_len, "%s", session_settings_vignette_size_name(session_settings.vignette_height));
            break;
        case row_offset_x:
            snprintf(buf, buf_len, "%s", session_settings_vignette_offset_name(session_settings.vignette_offset_x));
            break;
        case row_offset_y:
            snprintf(buf, buf_len, "%s", session_settings_vignette_offset_name(session_settings.vignette_offset_y));
            break;
        case row_softness:
            snprintf(buf, buf_len, "%s", session_settings_vignette_percent_name(session_settings.vignette_softness));
            break;
        case row_strength:
            snprintf(buf, buf_len, "%s", session_settings_vignette_percent_name(session_settings.vignette_strength));
            break;
        case row_colour:
            snprintf(buf, buf_len, "%s", session_settings_vignette_colour_name(session_settings.vignette_colour));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_shape:
            session_settings_cycle_vignette_shape(direction);
            break;
        case row_scaling:
            session_settings_cycle_vignette_scaling(direction);
            break;
        case row_width:
            session_settings_cycle_vignette_width(direction);
            break;
        case row_height:
            session_settings_cycle_vignette_height(direction);
            break;
        case row_offset_x:
            session_settings_cycle_vignette_offset_x(direction);
            break;
        case row_offset_y:
            session_settings_cycle_vignette_offset_y(direction);
            break;
        case row_softness:
            session_settings_cycle_vignette_softness(direction);
            break;
        case row_strength:
            session_settings_cycle_vignette_strength(direction);
            break;
        case row_colour:
            session_settings_cycle_vignette_colour(direction);
            break;
        default:
            break;
    }
}

static submenu self;

#define SECRET_HOLD_MS 1500

static uint32_t secret_since[2];
static int secret_holding[2];
static int secret_fired[2];

static int secret_tick(void) {
    static const int shapes[2] = {vignette_shape_flower, vignette_shape_triangle};
    const int held[2] = {mux_input_pressed(mux_input_x), mux_input_pressed(mux_input_y)};
    const int on_shape_row = current_item_index == row_shape;
    const uint32_t now = SDL_GetTicks();

    for (int i = 0; i < 2; i++) {
        if (!on_shape_row || !held[i]) {
            secret_holding[i] = 0;
            secret_fired[i] = 0;
            continue;
        }

        if (!secret_holding[i]) {
            secret_holding[i] = 1;
            secret_since[i] = now;
            continue;
        }

        if (secret_fired[i] || !SDL_TICKS_PASSED(now, secret_since[i] + SECRET_HOLD_MS)) continue;

        secret_fired[i] = 1;
        session_settings.vignette_shape = shapes[i];

        play_sound(snd_confirm);
        submenu_refresh_values(&self);
    }

    return 0;
}

static int row_coarse_step(const int index) {
    return index == row_shape || index == row_scaling || index == row_colour ? 0 : 5;
}

static void closed(void) {
    display_menu_reopen_vignette();
}

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_coarse_step = row_coarse_step,
    .child_tick = secret_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.vignette_title,
    .save_desc = lang.muxretro.save.vignette_desc,
};

void vignette_menu_init(void) {
    submenu_init(&self, &def);
}

void vignette_menu_open(void) {
    for (int i = 0; i < 2; i++) {
        secret_holding[i] = 0;
        secret_fired[i] = 0;
    }

    submenu_open(&self);
}

int vignette_menu_is_active(void) {
    return submenu_is_active(&self);
}

void vignette_menu_tick(void) {
    submenu_tick(&self);
}
