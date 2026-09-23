#pragma once

#include <limits.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include <common/base/options.h>

#define MUXTERM_VERSION "1.4.2"

#define MUXTERM_DEFAULT_WIDTH  640
#define MUXTERM_DEFAULT_HEIGHT 480

#define MUXTERM_DEFAULT_FONT_PATH "/opt/muos/share/font/muterm.ttf"

#define MUXTERM_DEFAULT_TERM_SIZE 15
#define MUXTERM_DEFAULT_MENU_SIZE 17

#define MUXTERM_DEFAULT_SCROLLBACK 512

#define MUXTERM_DEFAULT_SB_PATH "/tmp/mustardos/muxterm.cache"

#define MUXTERM_DEFAULT_KEY_DELAY  350
#define MUXTERM_DEFAULT_KEY_RATE   70
#define MUXTERM_DEFAULT_DPAD_DELAY 300
#define MUXTERM_DEFAULT_DPAD_RATE  80

#define MUOS_DEVICE_CONFIG OPT_PATH "device/config"

#define MUXTERM_USR_CONF ".config/muxterm/muxterm.conf"

typedef struct {
    int width;
    int height;

    char term_font_path[512];
    char term_font_path_bold[512];
    char term_font_path_italic[512];
    char term_font_path_bold_italic[512];

    int term_font_size;
    int menu_font_size;

    int scrollback;
    char scrollback_path[512];

    int use_solid_bg;
    SDL_Color solid_bg;

    int use_solid_fg;
    SDL_Color solid_fg;

    char bg_image[512];
    int readonly;

    float zoom;
    int rotate;
    int underscan;

    char shell[256];

    char osk_layout_path[512];

    int key_repeat_delay;
    int key_repeat_rate;
    int dpad_repeat_delay;
    int dpad_repeat_rate;

    int force_redraw;

    int font_hinting;

    int term_font_path_explicit;
    int term_font_size_explicit;
    int font_hinting_explicit;
    int foreground_explicit;
    int background_explicit;
    int background_image_explicit;

    int ignore_muos;

    char custom_config_path[PATH_MAX];
} MuxtermConfig;

struct mux_config;

void config_load(
    MuxtermConfig *cfg, const struct mux_config *muos_config, int ignore_muos, const char *custom_config_path
);

void config_dump(const MuxtermConfig *cfg);

int config_save_managed(const MuxtermConfig *cfg);

int config_reset_managed(MuxtermConfig *cfg, const struct mux_config *muos_config);
