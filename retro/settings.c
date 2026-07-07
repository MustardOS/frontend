#include <string.h>
#include "../common/fileio.h"
#include "../common/init.h"
#include "../common/language.h"
#include "../common/log.h"
#include "../common/mini/mini.h"
#include "../common/miniz/miniz.h"
#include "../common/options.h"
#include "../common/strutil.h"
#include "muxretro.h"
#include "paths.h"
#include "settings.h"

static const struct session_settings_t defaults = {
    .scaling_mode = video_scale_aspect,
    .texture_filter = texture_filter_nearest,
    .rumble_enabled = 1,
    .volume = 100,
    .show_fps = 0,
    .border_color = border_color_theme,
    .sample_rate = 0,
    .fps_limit = fps_limit_60,
};

struct session_settings_t session_settings;
static struct session_settings_t baseline_settings;

static char core_ini_path[MAX_BUFFER_SIZE] = "";
static char content_ini_path[MAX_BUFFER_SIZE] = "";
static char directory_ini_path[MAX_BUFFER_SIZE] = "";

static const char *scale_names[video_scale_count] = {
    lang.muxretro.settings_screen.aspect_ratio, lang.muxretro.settings_screen.integer_x1,
    lang.muxretro.settings_screen.integer_x2,   lang.muxretro.settings_screen.integer_x3,
    lang.muxretro.settings_screen.stretch,      lang.muxretro.settings_screen.integer_auto
};

static const char *filter_names[texture_filter_count] = {
    lang.muxretro.settings_screen.nearest, lang.muxretro.settings_screen.smooth, lang.muxretro.settings_screen.scale2_x
};

static const char *border_names[border_color_count] = {
    lang.muxretro.settings_screen.theme, lang.muxretro.settings_screen.black, lang.muxretro.settings_screen.dark_grey,
    lang.muxretro.settings_screen.white
};

static const int sample_rate_choices[] = {0, 44100, 48000};
#define SAMPLE_RATE_CHOICE_COUNT ((int) (sizeof(sample_rate_choices) / sizeof(sample_rate_choices[0])))

static const char *fps_limit_names[fps_limit_count] = {
    lang.muxretro.settings_screen.fps_60, lang.muxretro.settings_screen.fps_50, lang.muxretro.settings_screen.fps_none
};

const char *session_settings_scale_name(const int mode) {
    if (mode < 0 || mode >= video_scale_count) return scale_names[video_scale_aspect];
    return scale_names[mode];
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

static void apply_ini(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) return;

    long long v = mini_get_int(ini, "settings", "scaling_mode", -1);
    if (v >= 0 && v < video_scale_count) session_settings.scaling_mode = (int) v;

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

    mini_free(ini);
}

static void write_ini(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) ini = mini_create(path);
    if (!ini) return;

    mini_set_int(ini, "settings", "scaling_mode", session_settings.scaling_mode);
    mini_set_int(ini, "settings", "texture_filter", session_settings.texture_filter);
    mini_set_int(ini, "settings", "rumble_enabled", session_settings.rumble_enabled);
    mini_set_int(ini, "settings", "volume", session_settings.volume);
    mini_set_int(ini, "settings", "show_fps", session_settings.show_fps);
    mini_set_int(ini, "settings", "border_color", session_settings.border_color);
    mini_set_int(ini, "settings", "sample_rate", session_settings.sample_rate);
    mini_set_int(ini, "settings", "fps_limit", session_settings.fps_limit);

    mini_save(ini, 0);
    mini_free(ini);
}

void session_settings_init(const char *core_path_arg, const char *content_path) {
    session_settings = defaults;

    char core_name[MAX_BUFFER_SIZE];
    const char *core_base = strrchr(core_path_arg, '/');
    core_base = core_base ? core_base + 1 : core_path_arg;
    snprintf(core_name, sizeof(core_name), "%s", core_base);

    char *ext = strstr(core_name, "_libretro.so");
    if (ext) *ext = '\0';
    snprintf(core_ini_path, sizeof(core_ini_path), "%s/%s.ini", RETRO_SET_PATH, core_name);
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
