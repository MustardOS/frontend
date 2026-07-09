#include <string.h>
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/language.h"
#include "../common/log.h"
#include "../common/mini/mini.h"
#include "../common/miniz/miniz.h"
#include "../common/options.h"
#include "../common/strutil.h"
#include "colour.h"
#include "core.h"
#include "muxretro.h"
#include "paths.h"
#include "settings.h"

static const struct session_settings_t defaults = {
    .scaling_mode = video_scale_aspect,
    .aspect_ratio = aspect_ratio_auto,
    .integer_scale = integer_scale_auto,
    .texture_filter = texture_filter_nearest,
    .rumble_enabled = 1,
    .volume = 100,
    .show_fps = 0,
    .border_color = border_color_theme,
    .sample_rate = 0,
    .fps_limit = fps_limit_60,
    .header_visibility = header_visibility_none,
    .ff_speed = ff_speed_4_x,
    .slowmo_speed = slowmo_speed_1_2_x,
    .hotkey_ff_enabled = 1,
    .hotkey_slowmo_enabled = 1,
    .hotkey_quicksave_enabled = 1,
    .hotkey_quickload_enabled = 1,
    .sram_flush_seconds = 60,
    .colour_brightness = 0,
    .colour_contrast = 100,
    .colour_saturation = 100,
    .colour_hueshift = 0,
    .colour_gamma = 100,
    .colour_filter = 0,
    .colour_shader = 0,
};

#define COLOUR_BRIGHTNESS_MIN -100
#define COLOUR_BRIGHTNESS_MAX 100
#define COLOUR_CONTRAST_MIN   0
#define COLOUR_CONTRAST_MAX   200
#define COLOUR_SATURATION_MIN 0
#define COLOUR_SATURATION_MAX 200
#define COLOUR_HUESHIFT_MIN   -180
#define COLOUR_HUESHIFT_MAX   180
#define COLOUR_GAMMA_MIN      10
#define COLOUR_GAMMA_MAX      400
#define COLOUR_STEP           5

struct session_settings_t session_settings;
static struct session_settings_t baseline_settings;

static char core_ini_path[MAX_BUFFER_SIZE] = "";
static char content_ini_path[MAX_BUFFER_SIZE] = "";
static char directory_ini_path[MAX_BUFFER_SIZE] = "";

static const char *scale_names[video_scale_count] = {
    lang.muxretro.settings_screen.aspect_ratio, lang.muxretro.settings_screen.integer_mode,
    lang.muxretro.settings_screen.stretch
};

static const double integer_scale_values[integer_scale_count] = {0.0,  1.00, 1.25, 1.50, 1.75, 2.00, 2.25,
                                                                 2.50, 2.75, 3.00, 3.25, 3.50, 3.75, 4.00};

static const char *aspect_ratio_names[aspect_ratio_count] = {
    lang.muxretro.settings_screen.auto_rate,   lang.muxretro.settings_screen.ratio_4_3,
    lang.muxretro.settings_screen.ratio_8_7,   lang.muxretro.settings_screen.ratio_16_9,
    lang.muxretro.settings_screen.ratio_16_10, lang.muxretro.settings_screen.pixel_perfect
};

static const char *filter_names[texture_filter_count] = {
    lang.muxretro.settings_screen.nearest, lang.muxretro.settings_screen.smooth, lang.muxretro.settings_screen.scale2_x,
    lang.muxretro.settings_screen.scale3_x, lang.muxretro.settings_screen.sharp_bilinear
};

static const char *border_names[border_color_count] = {
    lang.muxretro.settings_screen.theme, lang.muxretro.settings_screen.black, lang.muxretro.settings_screen.dark_grey,
    lang.muxretro.settings_screen.white
};

static const int sample_rate_choices[] = {0, 44100, 48000};
#define SAMPLE_RATE_CHOICE_COUNT ((int) (sizeof(sample_rate_choices) / sizeof(sample_rate_choices[0])))

static const int sram_flush_choices[] = {15, 30, 60, 90, 120, 240, 300};
#define SRAM_FLUSH_CHOICE_COUNT ((int) (sizeof(sram_flush_choices) / sizeof(sram_flush_choices[0])))

static const char *fps_limit_names[fps_limit_count] = {
    lang.muxretro.settings_screen.fps_60, lang.muxretro.settings_screen.fps_50, lang.muxretro.settings_screen.fps_none
};

static const char *header_visibility_names[header_visibility_count] = {
    lang.muxretro.settings_screen.header_none, lang.muxretro.settings_screen.header_clock,
    lang.muxretro.settings_screen.header_battery, lang.muxretro.settings_screen.header_both
};

static const double ff_speed_values[ff_speed_count] = {2.0, 3.0, 4.0, 8.0};

static const double slowmo_speed_values[slowmo_speed_count] = {0.5, 0.25, 0.125};

const char *session_settings_scale_name(const int mode) {
    if (mode < 0 || mode >= video_scale_count) return scale_names[video_scale_aspect];
    return scale_names[mode];
}

const char *session_settings_aspect_ratio_name(const int mode) {
    if (mode < 0 || mode >= aspect_ratio_count) return aspect_ratio_names[aspect_ratio_auto];
    return aspect_ratio_names[mode];
}

const char *session_settings_integer_scale_name(const int mode) {
    if (mode <= integer_scale_auto || mode >= integer_scale_count) return lang.muxretro.settings_screen.auto_rate;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%.2fx", integer_scale_values[mode]);
    return buf;
}

double session_settings_integer_scale_value(const int mode) {
    if (mode <= integer_scale_auto || mode >= integer_scale_count) return 0.0;
    return integer_scale_values[mode];
}

const char *session_settings_filter_name(const int mode) {
    if (mode < 0 || mode >= texture_filter_count) return filter_names[texture_filter_nearest];
    return filter_names[mode];
}

const char *session_settings_border_name(const int mode) {
    if (mode < 0 || mode >= border_color_count) return border_names[border_color_theme];
    return border_names[mode];
}

const char *session_settings_sample_rate_name(const int rate) {
    static char buf[64];

    if (rate <= 0) {
        int freq = 0, channels = 0;
        audio_bridge_get_info(&freq, &channels);

        if (freq > 0) {
            snprintf(buf, sizeof(buf), "%s (%d Hz)", lang.muxretro.settings_screen.auto_rate, freq);
            return buf;
        }

        return lang.muxretro.settings_screen.auto_rate;
    }

    snprintf(buf, sizeof(buf), "%d Hz", rate);
    return buf;
}

const char *session_settings_fps_limit_name(const int mode) {
    if (mode < 0 || mode >= fps_limit_count) return fps_limit_names[fps_limit_60];
    return fps_limit_names[mode];
}

const char *session_settings_header_visibility_name(const int mode) {
    if (mode < 0 || mode >= header_visibility_count) return header_visibility_names[header_visibility_none];
    return header_visibility_names[mode];
}

const char *session_settings_ff_speed_name(const int mode) {
    static char buf[8];
    const int clamped = mode < 0 || mode >= ff_speed_count ? ff_speed_4_x : mode;
    snprintf(buf, sizeof(buf), "%.0fx", ff_speed_values[clamped]);
    return buf;
}

const char *session_settings_slowmo_speed_name(const int mode) {
    static char buf[16];
    const int clamped = mode < 0 || mode >= slowmo_speed_count ? slowmo_speed_1_2_x : mode;
    snprintf(buf, sizeof(buf), "1/%.0fx", 1.0 / slowmo_speed_values[clamped]);
    return buf;
}

double session_settings_ff_speed_value(const int mode) {
    if (mode < 0 || mode >= ff_speed_count) return ff_speed_values[ff_speed_4_x];
    return ff_speed_values[mode];
}

double session_settings_slowmo_speed_value(const int mode) {
    if (mode < 0 || mode >= slowmo_speed_count) return slowmo_speed_values[slowmo_speed_1_2_x];
    return slowmo_speed_values[mode];
}

const char *session_settings_sram_flush_name(const int seconds) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%ds", seconds);
    return buf;
}

const char *session_settings_colour_brightness_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%+d%%", value);
    return buf;
}

const char *session_settings_colour_contrast_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_colour_saturation_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_colour_hueshift_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%+d\xC2\xB0", value);
    return buf;
}

const char *session_settings_colour_gamma_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_colour_filter_name(const int index) {
    return colour_filter_preset_label(index);
}

const char *session_settings_colour_shader_name(const int index) {
    return colour_shader_label(index);
}

static void apply_ini(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) return;

    long long v = mini_get_int(ini, "settings", "scaling_mode", -1);
    if (v >= 0 && v < video_scale_count) session_settings.scaling_mode = (int) v;

    v = mini_get_int(ini, "settings", "aspect_ratio", -1);
    if (v >= 0 && v < aspect_ratio_count) session_settings.aspect_ratio = (int) v;

    v = mini_get_int(ini, "settings", "integer_scale", -1);
    if (v >= 0 && v < integer_scale_count) session_settings.integer_scale = (int) v;

    v = mini_get_int(ini, "settings", "texture_filter", -1);
    if (v >= 0 && v < texture_filter_count) session_settings.texture_filter = (int) v;

    v = mini_get_int(ini, "settings", "rumble_enabled", -1);
    if (v == 0 || v == 1) session_settings.rumble_enabled = (int) v;

    v = mini_get_int(ini, "settings", "volume", -1);
    if (v >= 0 && v <= 100) session_settings.volume = (int) v;

    v = mini_get_int(ini, "settings", "show_fps", -1);
    if (v == 0 || v == 1) session_settings.show_fps = (int) v;

    v = mini_get_int(ini, "settings", "border_color", -1);
    if (v >= 0 && v < border_color_count) session_settings.border_color = (int) v;

    v = mini_get_int(ini, "settings", "sample_rate", -1);
    for (int i = 0; v >= 0 && i < SAMPLE_RATE_CHOICE_COUNT; i++) {
        if (sample_rate_choices[i] == (int) v) {
            session_settings.sample_rate = (int) v;
            break;
        }
    }

    v = mini_get_int(ini, "settings", "fps_limit", -1);
    if (v >= 0 && v < fps_limit_count) session_settings.fps_limit = (int) v;

    v = mini_get_int(ini, "settings", "header_visibility", -1);
    if (v >= 0 && v < header_visibility_count) session_settings.header_visibility = (int) v;

    v = mini_get_int(ini, "settings", "ff_speed", -1);
    if (v >= 0 && v < ff_speed_count) session_settings.ff_speed = (int) v;

    v = mini_get_int(ini, "settings", "slowmo_speed", -1);
    if (v >= 0 && v < slowmo_speed_count) session_settings.slowmo_speed = (int) v;

    v = mini_get_int(ini, "settings", "hotkey_ff_enabled", -1);
    if (v == 0 || v == 1) session_settings.hotkey_ff_enabled = (int) v;

    v = mini_get_int(ini, "settings", "hotkey_slowmo_enabled", -1);
    if (v == 0 || v == 1) session_settings.hotkey_slowmo_enabled = (int) v;

    v = mini_get_int(ini, "settings", "hotkey_quicksave_enabled", -1);
    if (v == 0 || v == 1) session_settings.hotkey_quicksave_enabled = (int) v;

    v = mini_get_int(ini, "settings", "hotkey_quickload_enabled", -1);
    if (v == 0 || v == 1) session_settings.hotkey_quickload_enabled = (int) v;

    v = mini_get_int(ini, "settings", "sram_flush_seconds", -1);
    for (int i = 0; v >= 0 && i < SRAM_FLUSH_CHOICE_COUNT; i++) {
        if (sram_flush_choices[i] == (int) v) {
            session_settings.sram_flush_seconds = (int) v;
            break;
        }
    }

    v = mini_get_int(ini, "settings", "colour_brightness", COLOUR_BRIGHTNESS_MIN - 1);
    if (v >= COLOUR_BRIGHTNESS_MIN && v <= COLOUR_BRIGHTNESS_MAX) session_settings.colour_brightness = (int) v;

    v = mini_get_int(ini, "settings", "colour_contrast", COLOUR_CONTRAST_MIN - 1);
    if (v >= COLOUR_CONTRAST_MIN && v <= COLOUR_CONTRAST_MAX) session_settings.colour_contrast = (int) v;

    v = mini_get_int(ini, "settings", "colour_saturation", COLOUR_SATURATION_MIN - 1);
    if (v >= COLOUR_SATURATION_MIN && v <= COLOUR_SATURATION_MAX) session_settings.colour_saturation = (int) v;

    v = mini_get_int(ini, "settings", "colour_hueshift", COLOUR_HUESHIFT_MIN - 1);
    if (v >= COLOUR_HUESHIFT_MIN && v <= COLOUR_HUESHIFT_MAX) session_settings.colour_hueshift = (int) v;

    v = mini_get_int(ini, "settings", "colour_gamma", COLOUR_GAMMA_MIN - 1);
    if (v >= COLOUR_GAMMA_MIN && v <= COLOUR_GAMMA_MAX) session_settings.colour_gamma = (int) v;

    v = mini_get_int(ini, "settings", "colour_filter", -1);
    if (v >= 0 && v < colour_filter_preset_count()) session_settings.colour_filter = (int) v;

    v = mini_get_int(ini, "settings", "colour_shader", -1);
    if (v >= 0 && v < colour_shader_count()) session_settings.colour_shader = (int) v;

    mini_free(ini);
}

static void write_ini(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) ini = mini_create(path);
    if (!ini) return;

    mini_set_int(ini, "settings", "scaling_mode", session_settings.scaling_mode);
    mini_set_int(ini, "settings", "aspect_ratio", session_settings.aspect_ratio);
    mini_set_int(ini, "settings", "integer_scale", session_settings.integer_scale);
    mini_set_int(ini, "settings", "texture_filter", session_settings.texture_filter);
    mini_set_int(ini, "settings", "rumble_enabled", session_settings.rumble_enabled);
    mini_set_int(ini, "settings", "volume", session_settings.volume);
    mini_set_int(ini, "settings", "show_fps", session_settings.show_fps);
    mini_set_int(ini, "settings", "border_color", session_settings.border_color);
    mini_set_int(ini, "settings", "sample_rate", session_settings.sample_rate);
    mini_set_int(ini, "settings", "fps_limit", session_settings.fps_limit);
    mini_set_int(ini, "settings", "header_visibility", session_settings.header_visibility);
    mini_set_int(ini, "settings", "ff_speed", session_settings.ff_speed);
    mini_set_int(ini, "settings", "slowmo_speed", session_settings.slowmo_speed);
    mini_set_int(ini, "settings", "hotkey_ff_enabled", session_settings.hotkey_ff_enabled);
    mini_set_int(ini, "settings", "hotkey_slowmo_enabled", session_settings.hotkey_slowmo_enabled);
    mini_set_int(ini, "settings", "hotkey_quicksave_enabled", session_settings.hotkey_quicksave_enabled);
    mini_set_int(ini, "settings", "hotkey_quickload_enabled", session_settings.hotkey_quickload_enabled);
    mini_set_int(ini, "settings", "sram_flush_seconds", session_settings.sram_flush_seconds);
    mini_set_int(ini, "settings", "colour_brightness", session_settings.colour_brightness);
    mini_set_int(ini, "settings", "colour_contrast", session_settings.colour_contrast);
    mini_set_int(ini, "settings", "colour_saturation", session_settings.colour_saturation);
    mini_set_int(ini, "settings", "colour_hueshift", session_settings.colour_hueshift);
    mini_set_int(ini, "settings", "colour_gamma", session_settings.colour_gamma);
    mini_set_int(ini, "settings", "colour_filter", session_settings.colour_filter);
    mini_set_int(ini, "settings", "colour_shader", session_settings.colour_shader);

    mini_save(ini, 0);
    mini_free(ini);
}

void session_settings_init(const char *core_path_arg, const char *content_path) {
    colour_init();

    session_settings = defaults;

    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));
    snprintf(core_ini_path, sizeof(core_ini_path), "%s/core/%s.ini", RETRO_SET_PATH, core_name);
    create_directories(core_ini_path, 1);

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;
    snprintf(content_ini_path, sizeof(content_ini_path), "%s/content/%s.ini", RETRO_SET_PATH, content_base);
    create_directories(content_ini_path, 1);

    const char *content_dir = get_content_path((char *) content_path);
    const mz_ulong dir_crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char *) content_dir, strlen(content_dir));
    snprintf(
        directory_ini_path, sizeof(directory_ini_path), "%s/directory/%08lX.ini", RETRO_SET_PATH,
        (unsigned long) dir_crc
    );
    create_directories(directory_ini_path, 1);

    apply_ini(core_ini_path);
    apply_ini(directory_ini_path);
    apply_ini(content_ini_path);

    baseline_settings = session_settings;
}

void session_settings_cycle_scaling(const int direction) {
    session_settings.scaling_mode = (session_settings.scaling_mode + direction + video_scale_count) % video_scale_count;
    video_bridge_apply_scaling();
}

void session_settings_cycle_aspect_ratio(const int direction) {
    session_settings.aspect_ratio =
        (session_settings.aspect_ratio + direction + aspect_ratio_count) % aspect_ratio_count;
    video_bridge_apply_scaling();
}

void session_settings_cycle_integer_scale(const int direction) {
    session_settings.integer_scale =
        (session_settings.integer_scale + direction + integer_scale_count) % integer_scale_count;
    video_bridge_apply_scaling();
}

void session_settings_cycle_filter(const int direction) {
    session_settings.texture_filter =
        (session_settings.texture_filter + direction + texture_filter_count) % texture_filter_count;
    video_bridge_apply_filter();
}

void session_settings_cycle_rumble(const int direction) {
    (void) direction;
    session_settings.rumble_enabled = !session_settings.rumble_enabled;
}

void session_settings_cycle_volume(const int direction) {
    session_settings.volume += direction * 10;
    if (session_settings.volume < 0) session_settings.volume = 0;
    if (session_settings.volume > 100) session_settings.volume = 100;
}

void session_settings_cycle_fps(const int direction) {
    (void) direction;
    session_settings.show_fps = !session_settings.show_fps;
    pause_menu_set_fps_visible(session_settings.show_fps);
}

void session_settings_cycle_border(const int direction) {
    session_settings.border_color =
        (session_settings.border_color + direction + border_color_count) % border_color_count;
}

void session_settings_cycle_sample_rate(const int direction) {
    int idx = 0;
    for (int i = 0; i < SAMPLE_RATE_CHOICE_COUNT; i++) {
        if (sample_rate_choices[i] == session_settings.sample_rate) {
            idx = i;
            break;
        }
    }

    idx = (idx + direction + SAMPLE_RATE_CHOICE_COUNT) % SAMPLE_RATE_CHOICE_COUNT;
    session_settings.sample_rate = sample_rate_choices[idx];

    LOG_INFO(mux_module, "Sample Rate changed to %s", session_settings_sample_rate_name(session_settings.sample_rate));
    audio_bridge_apply_sample_rate();
}

void session_settings_cycle_fps_limit(const int direction) {
    session_settings.fps_limit = (session_settings.fps_limit + direction + fps_limit_count) % fps_limit_count;
    LOG_INFO(mux_module, "FPS Limit changed to %s", session_settings_fps_limit_name(session_settings.fps_limit));
    video_bridge_apply_fps_limit();
}

void session_settings_cycle_header_visibility(const int direction) {
    session_settings.header_visibility =
        (session_settings.header_visibility + direction + header_visibility_count) % header_visibility_count;
}

void session_settings_cycle_ff_speed(const int direction) {
    session_settings.ff_speed = (session_settings.ff_speed + direction + ff_speed_count) % ff_speed_count;
}

void session_settings_cycle_slowmo_speed(const int direction) {
    session_settings.slowmo_speed =
        (session_settings.slowmo_speed + direction + slowmo_speed_count) % slowmo_speed_count;
}

void session_settings_cycle_hotkey_ff_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_ff_enabled = !session_settings.hotkey_ff_enabled;
}

void session_settings_cycle_hotkey_slowmo_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_slowmo_enabled = !session_settings.hotkey_slowmo_enabled;
}

void session_settings_cycle_hotkey_quicksave_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_quicksave_enabled = !session_settings.hotkey_quicksave_enabled;
}

void session_settings_cycle_hotkey_quickload_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_quickload_enabled = !session_settings.hotkey_quickload_enabled;
}

void session_settings_cycle_sram_flush(const int direction) {
    int idx = 0;
    for (int i = 0; i < SRAM_FLUSH_CHOICE_COUNT; i++) {
        if (sram_flush_choices[i] == session_settings.sram_flush_seconds) {
            idx = i;
            break;
        }
    }

    idx = (idx + direction + SRAM_FLUSH_CHOICE_COUNT) % SRAM_FLUSH_CHOICE_COUNT;
    session_settings.sram_flush_seconds = sram_flush_choices[idx];
}

void session_settings_cycle_colour_brightness(const int direction) {
    session_settings.colour_brightness += direction * COLOUR_STEP;
    if (session_settings.colour_brightness < COLOUR_BRIGHTNESS_MIN)
        session_settings.colour_brightness = COLOUR_BRIGHTNESS_MIN;
    if (session_settings.colour_brightness > COLOUR_BRIGHTNESS_MAX)
        session_settings.colour_brightness = COLOUR_BRIGHTNESS_MAX;
    colour_refresh();
}

void session_settings_cycle_colour_contrast(const int direction) {
    session_settings.colour_contrast += direction * COLOUR_STEP;
    if (session_settings.colour_contrast < COLOUR_CONTRAST_MIN) session_settings.colour_contrast = COLOUR_CONTRAST_MIN;
    if (session_settings.colour_contrast > COLOUR_CONTRAST_MAX) session_settings.colour_contrast = COLOUR_CONTRAST_MAX;
    colour_refresh();
}

void session_settings_cycle_colour_saturation(const int direction) {
    session_settings.colour_saturation += direction * COLOUR_STEP;
    if (session_settings.colour_saturation < COLOUR_SATURATION_MIN)
        session_settings.colour_saturation = COLOUR_SATURATION_MIN;
    if (session_settings.colour_saturation > COLOUR_SATURATION_MAX)
        session_settings.colour_saturation = COLOUR_SATURATION_MAX;
    colour_refresh();
}

void session_settings_cycle_colour_hueshift(const int direction) {
    session_settings.colour_hueshift += direction * COLOUR_STEP;
    if (session_settings.colour_hueshift < COLOUR_HUESHIFT_MIN) session_settings.colour_hueshift = COLOUR_HUESHIFT_MAX;
    if (session_settings.colour_hueshift > COLOUR_HUESHIFT_MAX) session_settings.colour_hueshift = COLOUR_HUESHIFT_MIN;
    colour_refresh();
}

void session_settings_cycle_colour_gamma(const int direction) {
    session_settings.colour_gamma += direction * COLOUR_STEP;
    if (session_settings.colour_gamma < COLOUR_GAMMA_MIN) session_settings.colour_gamma = COLOUR_GAMMA_MIN;
    if (session_settings.colour_gamma > COLOUR_GAMMA_MAX) session_settings.colour_gamma = COLOUR_GAMMA_MAX;
    colour_refresh();
}

void session_settings_cycle_colour_filter(const int direction) {
    const int count = colour_filter_preset_count();
    session_settings.colour_filter = (session_settings.colour_filter + direction + count) % count;
    colour_refresh();
}

void session_settings_set_colour_filter(const int index) {
    if (index < 0 || index >= colour_filter_preset_count()) return;
    session_settings.colour_filter = index;
    colour_refresh();
}

void session_settings_set_colour_shader(const int index) {
    if (index < 0 || index >= colour_shader_count()) return;
    session_settings.colour_shader = index;
}

int session_settings_is_dirty(void) {
    return memcmp(&session_settings, &baseline_settings, sizeof(session_settings)) != 0;
}

void session_settings_discard(void) {
    session_settings = baseline_settings;
    video_bridge_apply_scaling();
    video_bridge_apply_filter();
    pause_menu_set_fps_visible(session_settings.show_fps);
    audio_bridge_apply_sample_rate();
    video_bridge_apply_fps_limit();
    colour_refresh();
}

void session_settings_save_content(void) {
    write_ini(content_ini_path);
    baseline_settings = session_settings;
}

void session_settings_save_core(void) {
    write_ini(core_ini_path);
    baseline_settings = session_settings;
}

void session_settings_save_directory(void) {
    write_ini(directory_ini_path);
    baseline_settings = session_settings;
}
