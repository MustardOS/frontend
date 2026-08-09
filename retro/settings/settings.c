#include <limits.h>
#include <string.h>
#include "../../common/device.h"
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/mini/mini.h"
#include "../../common/options.h"
#include "../../common/overlay.h"
#include "../../common/strutil.h"
#include "../video/colour.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../input/core_input_meta.h"
#include "../input/rumble.h"
#include "../macro/macro.h"
#include "../video/overlay_bridge.h"
#include "../core/paths.h"
#include "../core/perf.h"
#include "settings.h"

static const struct session_settings_t defaults = {
    .scaling_mode = video_scale_fit,
    .rotate = video_rotate_0,
    .mirrored = 0,
    .aspect_ratio = aspect_ratio_auto,
    .integer_scale = integer_scale_auto,
    .texture_filter = texture_filter_nearest,
    .rumble_enabled = 1,
    .volume = 100,
    .show_fps = 0,
    .show_playtime = 0,
    .content_precache = content_precache_off,
    .border_colour = border_colour_theme,
    .sample_rate = 0,
    .fps_limit = fps_limit_60,
    .header_visibility = header_visibility_none,
    .ff_speed = ff_speed_4_x,
    .slowmo_speed = slowmo_speed_1_2_x,
    .hotkey_ff_enabled = 1,
    .hotkey_ff_glyph_enabled = 1,
    .hotkey_slowmo_enabled = 1,
    .hotkey_slowmo_glyph_enabled = 1,
    .hotkey_pause_enabled = 1,
    .hotkey_pause_glyph_enabled = 1,
    .hotkey_quicksave_enabled = 1,
    .hotkey_quickload_enabled = 1,
    .hotkey_toggle_fps_enabled = 1,
    .hotkey_header_toggle_enabled = 1,
    .hotkey_quit_enabled = 1,
    .hotkey_manual_enabled = 1,
    .auto_save = auto_save_idle_quit,
    .sram_flush_seconds = 60,
    .sram_backup_enabled = 1,
    .timeline_interval = 0,
    .timeline_count = 3,
    .colour_brightness = 0,
    .colour_contrast = 100,
    .colour_saturation = 100,
    .colour_hueshift = 0,
    .colour_gamma = 100,
    .colour_filter = 0,
    .colour_shader = 0,
    .overlay_source = overlay_source_off,
    .overlay_pattern = 0,
    .overlay_opacity = 100,
    .viewport_offset_x = 0,
    .viewport_offset_y = 0,
    .viewport_stretch_x = 0,
    .viewport_stretch_y = 0,
    .viewport_zoom = 100,
    .viewport_crop_top = 0,
    .viewport_crop_bottom = 0,
    .viewport_crop_left = 0,
    .viewport_crop_right = 0,
    .viewport_centre_crop = 0,
    .frame_delay_ms = FRAME_DELAY_AUTO,
    .stick_deadzone = 15,
    .stick_anti_deadzone = 0,
    .stick_sensitivity = 100,
    .stick_invert_y = 0,
    .audio_latency_profile = audio_latency_balanced,
    .audio_period_frames = 512,
    .audio_filter = audio_filter_none,
    .audio_rate_control = 50,
    .game_renderer = game_renderer_hardware,
    .shimmer_fix = 0,
    .run_ahead = 0,
    .gpu_hard_sync = 0,
    .port_assignment = {port_assignment_remembered, port_assignment_auto, port_assignment_auto, port_assignment_auto},
    .port_device_key = {"builtin", "", "", ""},
    .port_device_id = {0, 0, 0, 0},
};

// Physical control per source index
const int session_settings_source_types[PORT_SOURCE_COUNT] = {
    mux_input_a,          mux_input_b,       mux_input_x,       mux_input_y,         mux_input_l1,
    mux_input_r1,         mux_input_l2,      mux_input_r2,      mux_input_l3,        mux_input_r3,
    mux_input_select,     mux_input_start,   mux_input_dpad_up, mux_input_dpad_down, mux_input_dpad_left,
    mux_input_dpad_right, mux_input_ls_up,   mux_input_ls_down, mux_input_ls_left,   mux_input_ls_right,
    mux_input_rs_up,      mux_input_rs_down, mux_input_rs_left, mux_input_rs_right,
};

// Default core target per source index (-1 = unbound)
static const int default_source_target[PORT_SOURCE_COUNT] = {8, 0, 9, 1, 10, 11, 12, 13, 14, 15, 2,  3,
                                                             4, 5, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1};

static struct session_settings_t default_settings(void) {
    struct session_settings_t out = defaults;

    for (int port = 0; port < MUX_INPUT_PORT_COUNT; port++) {
        out.port_device_id[port] = (int) core_input_meta_preferred_device(port);

        for (int source = 0; source < PORT_SOURCE_COUNT; source++) {
            out.port_source_target[port][source] = default_source_target[source];
            out.port_source_macro[port][source] = -1;
        }
    }

    return out;
}

static const int default_button_map[16] = {
    mux_input_b,       mux_input_y,         mux_input_select,    mux_input_start,
    mux_input_dpad_up, mux_input_dpad_down, mux_input_dpad_left, mux_input_dpad_right,
    mux_input_a,       mux_input_x,         mux_input_l1,        mux_input_r1,
    mux_input_l2,      mux_input_r2,        mux_input_l3,        mux_input_r3,
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

#define VIEWPORT_OFFSET_STEP   1
#define VIEWPORT_STRETCH_STEP  1
#define VIEWPORT_ZOOM_MIN      25
#define VIEWPORT_ZOOM_MAX      300
#define VIEWPORT_ZOOM_STEP     5
#define VIEWPORT_CROP_STEP     1
#define VIEWPORT_CROP_MAX      512
#define VIEWPORT_CROP_MIN_KEEP 16

#define STICK_DEADZONE_MIN      0
#define STICK_DEADZONE_MAX      50
#define STICK_ANTI_DEADZONE_MIN 0
#define STICK_ANTI_DEADZONE_MAX 50
#define STICK_SENSITIVITY_MIN   50
#define STICK_SENSITIVITY_MAX   200
#define STICK_STEP              5

struct session_settings_t session_settings;
static struct session_settings_t baseline_settings;

static char core_ini_path[MAX_BUFFER_SIZE] = "";
static char content_ini_path[MAX_BUFFER_SIZE] = "";
static char directory_ini_path[MAX_BUFFER_SIZE] = "";

static const char *scale_names[video_scale_count] = {
    lang.muxretro.settings_screen.aspect_ratio, lang.muxretro.settings_screen.integer_mode,
    lang.muxretro.settings_screen.stretch,      lang.muxretro.settings_screen.full_height,
    lang.muxretro.settings_screen.full_width,   lang.muxretro.settings_screen.fit_screen
};

static const int rotate_degrees[video_rotate_count] = {0, 90, 180, 270};

static const double integer_scale_values[integer_scale_count] = {0.0,  1.00, 1.25, 1.50, 1.75, 2.00, 2.25,
                                                                 2.50, 2.75, 3.00, 3.25, 3.50, 3.75, 4.00};

static const char *aspect_ratio_names[aspect_ratio_count] = {
    lang.muxretro.settings_screen.auto_rate,   lang.muxretro.settings_screen.ratio_4_3,
    lang.muxretro.settings_screen.ratio_8_7,   lang.muxretro.settings_screen.ratio_16_9,
    lang.muxretro.settings_screen.ratio_16_10, lang.muxretro.settings_screen.pixel_perfect
};

static const char *filter_names[texture_filter_count] = {
    lang.muxretro.settings_screen.nearest,        lang.muxretro.settings_screen.smooth,
    lang.muxretro.settings_screen.scale2_x,       lang.muxretro.settings_screen.scale3_x,
    lang.muxretro.settings_screen.sharp_bilinear, lang.muxretro.settings_screen.scale2_x_smooth,
    lang.muxretro.settings_screen.super_eagle
};

static const char *game_renderer_names[game_renderer_count] = {
    lang.muxretro.settings_screen.game_renderer_hardware, lang.muxretro.settings_screen.game_renderer_software
};

static const char *audio_filter_names[audio_filter_count] = {
    lang.muxretro.settings_screen.audio_filter_none, lang.muxretro.settings_screen.audio_filter_low_pass,
    lang.muxretro.settings_screen.audio_filter_high_pass
};

static const char *border_names[border_colour_count] = {
    lang.muxretro.settings_screen.theme, lang.muxretro.settings_screen.black, lang.muxretro.settings_screen.dark_grey,
    lang.muxretro.settings_screen.white
};

static const int sample_rate_choices[] = {0, 44100, 48000};
#define SAMPLE_RATE_CHOICE_COUNT ((int) (sizeof(sample_rate_choices) / sizeof(sample_rate_choices[0])))

static const int audio_period_choices[] = {128, 256, 512, 1024, 2048};
#define AUDIO_PERIOD_CHOICE_COUNT ((int) (sizeof(audio_period_choices) / sizeof(audio_period_choices[0])))

static const int audio_rate_control_choices[] = {0, 25, 50, 100, 200};
#define AUDIO_RATE_CONTROL_CHOICE_COUNT                                                                                \
    ((int) (sizeof(audio_rate_control_choices) / sizeof(audio_rate_control_choices[0])))

static const int sram_flush_choices[] = {15, 30, 60, 90, 120, 240, 300};
#define SRAM_FLUSH_CHOICE_COUNT ((int) (sizeof(sram_flush_choices) / sizeof(sram_flush_choices[0])))

static const int frame_delay_choices[] = {FRAME_DELAY_OFF, FRAME_DELAY_AUTO, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16};
#define FRAME_DELAY_CHOICE_COUNT ((int) (sizeof(frame_delay_choices) / sizeof(frame_delay_choices[0])))

enum setting_validation {
    setting_range,
    setting_choices,
    setting_colour_filter,
    setting_colour_shader,
    setting_overlay_pattern,
    setting_viewport_x,
    setting_viewport_y
};

struct setting_descriptor {
    const char *key;
    size_t offset;
    enum setting_validation validation;
    int minimum;
    int maximum;
    const int *choices;
    size_t choice_count;
};

#define SETTING_RANGE(FIELD, MINIMUM, MAXIMUM)                                                                         \
    {#FIELD, offsetof(struct session_settings_t, FIELD), setting_range, MINIMUM, MAXIMUM, NULL, 0}
#define SETTING_CHOICES(FIELD, CHOICES)                                                                                \
    {#FIELD,  offsetof(struct session_settings_t, FIELD), setting_choices, 0, 0,                                       \
     CHOICES, sizeof(CHOICES) / sizeof((CHOICES)[0])}
#define SETTING_SPECIAL(FIELD, VALIDATION)                                                                             \
    {#FIELD, offsetof(struct session_settings_t, FIELD), VALIDATION, 0, 0, NULL, 0}

static const struct setting_descriptor setting_descriptors[] = {
    SETTING_RANGE(scaling_mode, 0, video_scale_count - 1),
    SETTING_RANGE(rotate, 0, video_rotate_count - 1),
    SETTING_RANGE(mirrored, 0, 1),
    SETTING_RANGE(aspect_ratio, 0, aspect_ratio_count - 1),
    SETTING_RANGE(integer_scale, 0, integer_scale_count - 1),
    SETTING_RANGE(texture_filter, 0, texture_filter_count - 1),
    SETTING_RANGE(rumble_enabled, 0, 1),
    SETTING_RANGE(volume, 0, 100),
    SETTING_RANGE(show_fps, 0, show_fps_count - 1),
    SETTING_RANGE(show_playtime, 0, 1),
    SETTING_RANGE(content_precache, 0, content_precache_count - 1),
    SETTING_RANGE(border_colour, 0, border_colour_count - 1),
    SETTING_CHOICES(sample_rate, sample_rate_choices),
    SETTING_RANGE(fps_limit, 0, fps_limit_count - 1),
    SETTING_RANGE(header_visibility, 0, header_visibility_count - 1),
    SETTING_RANGE(ff_speed, 0, ff_speed_count - 1),
    SETTING_RANGE(slowmo_speed, 0, slowmo_speed_count - 1),
    SETTING_RANGE(hotkey_ff_enabled, 0, 1),
    SETTING_RANGE(hotkey_ff_glyph_enabled, 0, 1),
    SETTING_RANGE(hotkey_slowmo_enabled, 0, 1),
    SETTING_RANGE(hotkey_slowmo_glyph_enabled, 0, 1),
    SETTING_RANGE(hotkey_pause_enabled, 0, 1),
    SETTING_RANGE(hotkey_pause_glyph_enabled, 0, 1),
    SETTING_RANGE(hotkey_quicksave_enabled, 0, 1),
    SETTING_RANGE(hotkey_quickload_enabled, 0, 1),
    SETTING_RANGE(hotkey_toggle_fps_enabled, 0, 1),
    SETTING_RANGE(hotkey_header_toggle_enabled, 0, 1),
    SETTING_RANGE(hotkey_quit_enabled, 0, 1),
    SETTING_RANGE(hotkey_manual_enabled, 0, 1),
    SETTING_RANGE(auto_save, 0, auto_save_count - 1),
    SETTING_CHOICES(sram_flush_seconds, sram_flush_choices),
    SETTING_RANGE(sram_backup_enabled, 0, 1),
    SETTING_RANGE(timeline_interval, 0, 6),
    SETTING_RANGE(timeline_count, 2, 10),
    SETTING_RANGE(colour_brightness, COLOUR_BRIGHTNESS_MIN, COLOUR_BRIGHTNESS_MAX),
    SETTING_RANGE(colour_contrast, COLOUR_CONTRAST_MIN, COLOUR_CONTRAST_MAX),
    SETTING_RANGE(colour_saturation, COLOUR_SATURATION_MIN, COLOUR_SATURATION_MAX),
    SETTING_RANGE(colour_hueshift, COLOUR_HUESHIFT_MIN, COLOUR_HUESHIFT_MAX),
    SETTING_RANGE(colour_gamma, COLOUR_GAMMA_MIN, COLOUR_GAMMA_MAX),
    SETTING_SPECIAL(colour_filter, setting_colour_filter),
    SETTING_SPECIAL(colour_shader, setting_colour_shader),
    SETTING_RANGE(overlay_source, 0, overlay_source_count - 1),
    SETTING_SPECIAL(overlay_pattern, setting_overlay_pattern),
    SETTING_RANGE(overlay_opacity, 0, 100),
    SETTING_SPECIAL(viewport_offset_x, setting_viewport_x),
    SETTING_SPECIAL(viewport_offset_y, setting_viewport_y),
    SETTING_SPECIAL(viewport_stretch_x, setting_viewport_x),
    SETTING_SPECIAL(viewport_stretch_y, setting_viewport_y),
    SETTING_RANGE(viewport_zoom, VIEWPORT_ZOOM_MIN, VIEWPORT_ZOOM_MAX),
    SETTING_RANGE(viewport_crop_top, 0, VIEWPORT_CROP_MAX),
    SETTING_RANGE(viewport_crop_bottom, 0, VIEWPORT_CROP_MAX),
    SETTING_RANGE(viewport_crop_left, 0, VIEWPORT_CROP_MAX),
    SETTING_RANGE(viewport_crop_right, 0, VIEWPORT_CROP_MAX),
    SETTING_RANGE(viewport_centre_crop, 0, 1),
    SETTING_CHOICES(frame_delay_ms, frame_delay_choices),
    SETTING_RANGE(stick_deadzone, STICK_DEADZONE_MIN, STICK_DEADZONE_MAX),
    SETTING_RANGE(stick_anti_deadzone, STICK_ANTI_DEADZONE_MIN, STICK_ANTI_DEADZONE_MAX),
    SETTING_RANGE(stick_sensitivity, STICK_SENSITIVITY_MIN, STICK_SENSITIVITY_MAX),
    SETTING_RANGE(stick_invert_y, 0, 1),
    SETTING_RANGE(audio_latency_profile, 0, audio_latency_count - 1),
    SETTING_CHOICES(audio_period_frames, audio_period_choices),
    SETTING_RANGE(audio_filter, 0, audio_filter_count - 1),
    SETTING_CHOICES(audio_rate_control, audio_rate_control_choices),
    SETTING_RANGE(game_renderer, 0, game_renderer_count - 1),
    SETTING_RANGE(shimmer_fix, 0, 1),
    SETTING_RANGE(run_ahead, 0, 1),
    SETTING_RANGE(gpu_hard_sync, 0, 1),
};

#undef SETTING_SPECIAL
#undef SETTING_CHOICES
#undef SETTING_RANGE

static const char *fps_limit_names[fps_limit_count] = {
    lang.muxretro.settings_screen.fps_60, lang.muxretro.settings_screen.fps_50, lang.muxretro.settings_screen.fps_none
};

static const char *audio_latency_names[audio_latency_count] = {
    lang.muxretro.settings_screen.audio_latency_low, lang.muxretro.settings_screen.audio_latency_balanced,
    lang.muxretro.settings_screen.audio_latency_compat
};

static const char *show_fps_names[show_fps_count] = {
    lang.generic.disabled, lang.muxretro.settings_screen.show_fps_simple,
    lang.muxretro.settings_screen.show_fps_detailed
};

static const char *header_visibility_names[header_visibility_count] = {
    lang.muxretro.settings_screen.header_none, lang.muxretro.settings_screen.header_clock,
    lang.muxretro.settings_screen.header_battery, lang.muxretro.settings_screen.header_both
};

static const char *auto_save_names[auto_save_count] = {
    lang.generic.disabled, lang.muxretro.settings_screen.auto_save_idle, lang.muxretro.settings_screen.auto_save_quit,
    lang.muxretro.settings_screen.auto_save_idle_quit
};

static const int content_precache_mb[content_precache_count] = {0, 64, 128, 256};

static const double ff_speed_values[ff_speed_count] = {2.0, 3.0, 4.0, 8.0};

static const double slowmo_speed_values[slowmo_speed_count] = {0.5, 0.25, 0.125};

static const char *overlay_source_names[overlay_source_count] = {
    lang.generic.disabled, lang.muxretro.settings_screen.overlay_pattern_mode,
    lang.muxretro.settings_screen.overlay_catalogue_mode
};

static const char *overlay_pattern_names[] = {
    lang.muxretro.overlay_screen.checkerboard_1, lang.muxretro.overlay_screen.checkerboard_4,
    lang.muxretro.overlay_screen.diagonal_1,     lang.muxretro.overlay_screen.diagonal_2,
    lang.muxretro.overlay_screen.diagonal_4,     lang.muxretro.overlay_screen.lattice_1,
    lang.muxretro.overlay_screen.lattice_4,      lang.muxretro.overlay_screen.horizontal_1,
    lang.muxretro.overlay_screen.horizontal_2,   lang.muxretro.overlay_screen.horizontal_4,
    lang.muxretro.overlay_screen.vertical_1,     lang.muxretro.overlay_screen.vertical_2,
    lang.muxretro.overlay_screen.vertical_4,
};
#define OVERLAY_PATTERN_NAME_COUNT ((int) (sizeof(overlay_pattern_names) / sizeof(overlay_pattern_names[0])))

const char *session_settings_scale_name(const int mode) {
    if (mode < 0 || mode >= video_scale_count) return scale_names[video_scale_aspect];
    return scale_names[mode];
}

const char *session_settings_rotate_name(const int mode) {
    static char buf[8];
    const int clamped = mode < 0 || mode >= video_rotate_count ? video_rotate_0 : mode;
    snprintf(buf, sizeof(buf), "%d\xC2\xB0", rotate_degrees[clamped]);
    return buf;
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

const char *session_settings_audio_filter_name(const int mode) {
    if (mode < 0 || mode >= audio_filter_count) return audio_filter_names[audio_filter_none];
    return audio_filter_names[mode];
}

const char *session_settings_border_name(const int mode) {
    if (mode < 0 || mode >= border_colour_count) return border_names[border_colour_theme];
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

const char *session_settings_show_fps_name(const int mode) {
    if (mode < 0 || mode >= show_fps_count) return show_fps_names[show_fps_off];
    return show_fps_names[mode];
}

const char *session_settings_content_precache_name(const int mode) {
    static char buf[16];
    if (mode <= content_precache_off || mode >= content_precache_count) return lang.generic.disabled;

    snprintf(buf, sizeof(buf), "%d MB", content_precache_mb[mode]);
    return buf;
}

int session_settings_content_precache_mb(const int mode) {
    if (mode <= content_precache_off || mode >= content_precache_count) return 0;
    return content_precache_mb[mode];
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

static const int timeline_interval_minutes[7] = {0, 5, 10, 15, 30, 45, 60};

const char *session_settings_timeline_interval_name(const int mode) {
    if (mode <= 0 || mode > 6) return lang.generic.disabled;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%dm", timeline_interval_minutes[mode]);
    return buf;
}

int session_settings_timeline_interval_ms(void) {
    const int mode = session_settings.timeline_interval;
    if (mode <= 0 || mode > 6) return 0;
    return timeline_interval_minutes[mode] * 60 * 1000;
}

void session_settings_cycle_timeline_interval(const int direction) {
    session_settings.timeline_interval = (session_settings.timeline_interval + direction + 7) % 7;
}

void session_settings_cycle_timeline_count(const int direction) {
    int next = session_settings.timeline_count + (direction > 0 ? 1 : -1);
    if (next < 2) next = 10;
    if (next > 10) next = 2;
    session_settings.timeline_count = next;
}

const char *session_settings_auto_save_name(const int mode) {
    if (mode < 0 || mode >= auto_save_count) return auto_save_names[auto_save_off];
    return auto_save_names[mode];
}

int session_settings_auto_save_on_idle(void) {
    return session_settings.auto_save == auto_save_idle || session_settings.auto_save == auto_save_idle_quit;
}

int session_settings_auto_save_on_quit(void) {
    return session_settings.auto_save == auto_save_quit || session_settings.auto_save == auto_save_idle_quit;
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

const char *session_settings_overlay_source_name(const int mode) {
    if (mode < 0 || mode >= overlay_source_count) return overlay_source_names[overlay_source_off];
    return overlay_source_names[mode];
}

const char *session_settings_overlay_pattern_name(const int index) {
    if (index >= 0 && index < OVERLAY_PATTERN_NAME_COUNT) return overlay_pattern_names[index];
    return overlay_pattern_name(index);
}

const char *session_settings_overlay_opacity_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_viewport_offset_x_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%+dpx", value);
    return buf;
}

const char *session_settings_viewport_offset_y_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%+dpx", value);
    return buf;
}

const char *session_settings_viewport_stretch_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%+dpx", value);
    return buf;
}

const char *session_settings_viewport_zoom_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_viewport_crop_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%dpx", value);
    return buf;
}

const char *session_settings_frame_delay_name(const int value) {
    if (value == FRAME_DELAY_AUTO) return lang.muxretro.settings_screen.auto_rate;
    if (value <= FRAME_DELAY_OFF) return lang.generic.disabled;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%dms", value);
    return buf;
}

const char *session_settings_stick_deadzone_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_stick_anti_deadzone_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_stick_sensitivity_name(const int value) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", value);
    return buf;
}

const char *session_settings_audio_latency_name(const int mode) {
    if (mode < 0 || mode >= audio_latency_count) return audio_latency_names[audio_latency_balanced];
    return audio_latency_names[mode];
}

const char *session_settings_audio_period_name(const int frames) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d", frames);
    return buf;
}

const char *session_settings_game_renderer_name(const int mode) {
    if (mode < 0 || mode >= game_renderer_count) return game_renderer_names[game_renderer_hardware];
    return game_renderer_names[mode];
}

const char *session_settings_audio_rate_control_name(const int hundredths) {
    if (hundredths <= 0) return lang.muxretro.settings_screen.audio_rate_control_off;

    static char buf[16];
    snprintf(buf, sizeof(buf), "%d.%02d%%", hundredths / 100, hundredths % 100);
    return buf;
}

static int setting_value_valid(const struct setting_descriptor *descriptor, const long long value) {
    switch (descriptor->validation) {
        case setting_range:
            return value >= descriptor->minimum && value <= descriptor->maximum;
        case setting_choices:
            for (size_t i = 0; i < descriptor->choice_count; i++) {
                if (value == descriptor->choices[i]) return 1;
            }
            return 0;
        case setting_colour_filter:
            return value >= 0 && value < colour_filter_preset_count();
        case setting_colour_shader:
            return value >= 0 && value < colour_shader_count();
        case setting_overlay_pattern:
            return value >= 0 && value < overlay_pattern_count();
        case setting_viewport_x:
            return value >= -(device.mux.width / 2) && value <= device.mux.width / 2;
        case setting_viewport_y:
            return value >= -(device.mux.height / 2) && value <= device.mux.height / 2;
    }

    return 0;
}

static int *setting_field(struct session_settings_t *settings, const struct setting_descriptor *descriptor) {
    return (int *) ((unsigned char *) settings + descriptor->offset);
}

static const int *
setting_field_const(const struct session_settings_t *settings, const struct setting_descriptor *descriptor) {
    return (const int *) ((const unsigned char *) settings + descriptor->offset);
}

static void apply_scalar_settings(mini_t *ini) {
    for (size_t i = 0; i < sizeof(setting_descriptors) / sizeof(setting_descriptors[0]); i++) {
        const struct setting_descriptor *descriptor = &setting_descriptors[i];
        const long long value = mini_get_int(ini, "settings", descriptor->key, INT_MIN);
        if (setting_value_valid(descriptor, value)) *setting_field(&session_settings, descriptor) = (int) value;
    }
}

static void apply_ini(const char *path) {
    mini_t *ini = mini_try_load(path);
    if (!ini) return;

    apply_scalar_settings(ini);
    long long v;

    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        char key[32];

        snprintf(key, sizeof(key), "port%d_assignment", i);
        v = mini_get_int(ini, "settings", key, -1);
        if (v >= port_assignment_auto && v <= port_assignment_remembered) session_settings.port_assignment[i] = (int) v;

        snprintf(key, sizeof(key), "port%d_device_key", i);
        const char *device_key = mini_get_string(ini, "settings", key, NULL);
        if (device_key)
            snprintf(
                session_settings.port_device_key[i], sizeof(session_settings.port_device_key[i]), "%s", device_key
            );

        snprintf(key, sizeof(key), "port%d_device_id", i);
        v = mini_get_int(ini, "settings", key, -1);
        if (v >= 0) session_settings.port_device_id[i] = (int) v;

        for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
            snprintf(key, sizeof(key), "port%d_src_%d", i, s);
            v = mini_get_int(ini, "settings", key, -99);
            if (v >= -1 && v < PORT_TARGET_COUNT) session_settings.port_source_target[i][s] = (int) v;

            snprintf(key, sizeof(key), "port%d_srcms_%d", i, s);
            v = mini_get_int(ini, "settings", key, -1);
            if (v >= 0 && v <= 65535) session_settings.port_source_turbo[i][s] = (int) v;

            snprintf(key, sizeof(key), "port%d_macro_%d", i, s);
            v = mini_get_int(ini, "settings", key, -99);
            if (v >= -1 && v < MACRO_MAX) session_settings.port_source_macro[i][s] = (int) v;
        }
    }

    mini_free(ini);
}

static struct session_settings_t tier_base(const int with_core, const int with_directory) {
    const struct session_settings_t live = session_settings;

    session_settings = default_settings();
    if (with_core) apply_ini(core_ini_path);
    if (with_directory) apply_ini(directory_ini_path);

    const struct session_settings_t base = session_settings;
    session_settings = live;

    return base;
}

static void write_ini_delta(const char *path, const struct session_settings_t *base) {
    remove(path);

    if (memcmp(&session_settings, base, sizeof(session_settings)) == 0) return;

    mini_t *ini = mini_create(path);
    if (!ini) return;

    for (size_t i = 0; i < sizeof(setting_descriptors) / sizeof(setting_descriptors[0]); i++) {
        const struct setting_descriptor *descriptor = &setting_descriptors[i];
        const int value = *setting_field_const(&session_settings, descriptor);
        if (value != *setting_field_const(base, descriptor)) mini_set_int(ini, "settings", descriptor->key, value);
    }

    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        char key[32];

        if (session_settings.port_assignment[i] != base->port_assignment[i]) {
            snprintf(key, sizeof(key), "port%d_assignment", i);
            mini_set_int(ini, "settings", key, session_settings.port_assignment[i]);
        }

        if (strcmp(session_settings.port_device_key[i], base->port_device_key[i]) != 0) {
            snprintf(key, sizeof(key), "port%d_device_key", i);
            mini_set_string(ini, "settings", key, session_settings.port_device_key[i]);
        }

        if (session_settings.port_device_id[i] != base->port_device_id[i]) {
            snprintf(key, sizeof(key), "port%d_device_id", i);
            mini_set_int(ini, "settings", key, session_settings.port_device_id[i]);
        }

        for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
            if (session_settings.port_source_target[i][s] != base->port_source_target[i][s]) {
                snprintf(key, sizeof(key), "port%d_src_%d", i, s);
                mini_set_int(ini, "settings", key, session_settings.port_source_target[i][s]);
            }

            if (session_settings.port_source_turbo[i][s] != base->port_source_turbo[i][s]) {
                snprintf(key, sizeof(key), "port%d_srcms_%d", i, s);
                mini_set_int(ini, "settings", key, session_settings.port_source_turbo[i][s]);
            }

            if (session_settings.port_source_macro[i][s] != base->port_source_macro[i][s]) {
                snprintf(key, sizeof(key), "port%d_macro_%d", i, s);
                mini_set_int(ini, "settings", key, session_settings.port_source_macro[i][s]);
            }
        }
    }

    mini_save(ini, 0);
    mini_free(ini);
}

void session_settings_init(const char *core_path_arg, const char *content_path) {
    colour_init();

    session_settings = default_settings();

    char core_name[MAX_BUFFER_SIZE];
    if (!core_get_name(core_path_arg, core_name, sizeof(core_name))
        || !str_format_checked(core_ini_path, sizeof(core_ini_path), "%s/core/%s.ini", RETRO_SET_PATH, core_name)) {
        LOG_ERROR(mux_module, "Settings core path is too long");
        baseline_settings = session_settings;
        return;
    }
    create_directories(core_ini_path, 1);

    char rel_dir[MAX_BUFFER_SIZE];
    if (!core_content_rel_dir(content_path, rel_dir, sizeof(rel_dir))) {
        LOG_ERROR(mux_module, "Settings directory path is too long");
        baseline_settings = session_settings;
        return;
    }

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    if (!str_copy_checked(content_stem, sizeof(content_stem), content_base)) {
        LOG_ERROR(mux_module, "Settings content name is too long");
        baseline_settings = session_settings;
        return;
    }
    char *content_dot = strrchr(content_stem, '.');
    if (content_dot) *content_dot = '\0';

    if (*rel_dir) {
        if (!str_format_checked(
                content_ini_path, sizeof(content_ini_path), "%s/content/%s/%s.ini", RETRO_SET_PATH, rel_dir,
                content_stem
            )
            || !str_format_checked(
                directory_ini_path, sizeof(directory_ini_path), "%s/directory/%s/directory.ini", RETRO_SET_PATH, rel_dir
            )) {
            LOG_ERROR(mux_module, "Settings hierarchy path is too long");
            baseline_settings = session_settings;
            return;
        }
    } else {
        if (!str_format_checked(
                content_ini_path, sizeof(content_ini_path), "%s/content/%s.ini", RETRO_SET_PATH, content_stem
            )
            || !str_format_checked(
                directory_ini_path, sizeof(directory_ini_path), "%s/directory/directory.ini", RETRO_SET_PATH
            )) {
            LOG_ERROR(mux_module, "Settings hierarchy path is too long");
            baseline_settings = session_settings;
            return;
        }
    }
    create_directories(content_ini_path, 1);
    create_directories(directory_ini_path, 1);

    apply_ini(core_ini_path);
    apply_ini(directory_ini_path);
    apply_ini(content_ini_path);

    session_settings_apply_fps_mode();

    baseline_settings = session_settings;
}

void session_settings_cycle_scaling(const int direction) {
    session_settings.scaling_mode = (session_settings.scaling_mode + direction + video_scale_count) % video_scale_count;
    video_bridge_apply_scaling();
}

void session_settings_cycle_rotate(const int direction) {
    session_settings.rotate = (session_settings.rotate + direction + video_rotate_count) % video_rotate_count;
    video_bridge_apply_scaling();
}

void session_settings_cycle_mirrored(const int direction) {
    (void) direction;
    session_settings.mirrored = !session_settings.mirrored;
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

void session_settings_cycle_audio_filter(const int direction) {
    session_settings.audio_filter =
        (session_settings.audio_filter + direction + audio_filter_count) % audio_filter_count;
    LOG_INFO(
        mux_module, "Audio Filter changed to %s", session_settings_audio_filter_name(session_settings.audio_filter)
    );
    audio_bridge_apply_filter();
}

void session_settings_cycle_rumble(const int direction) {
    (void) direction;
    session_settings.rumble_enabled = !session_settings.rumble_enabled;
    rumble_bridge_refresh();
}

void session_settings_cycle_volume(const int direction) {
    session_settings.volume += direction * 10;
    if (session_settings.volume < 0) session_settings.volume = 0;
    if (session_settings.volume > 100) session_settings.volume = 100;
}

void session_settings_apply_fps_mode(void) {
    pause_menu_set_fps_visible(session_settings.show_fps != show_fps_off);
    perf_set_hud_active(session_settings.show_fps == show_fps_detailed);
    pause_menu_set_fps_text("");
}

void session_settings_cycle_fps(const int direction) {
    const int step = direction ? direction : 1;
    session_settings.show_fps = (session_settings.show_fps + step + show_fps_count) % show_fps_count;
    session_settings_apply_fps_mode();
}

void session_settings_cycle_show_playtime(const int direction) {
    (void) direction;
    session_settings.show_playtime = !session_settings.show_playtime;
}

void session_settings_cycle_content_precache(const int direction) {
    session_settings.content_precache =
        (session_settings.content_precache + direction + content_precache_count) % content_precache_count;
}

void session_settings_cycle_border(const int direction) {
    session_settings.border_colour =
        (session_settings.border_colour + direction + border_colour_count) % border_colour_count;
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

void session_settings_cycle_audio_period(const int direction) {
    int idx = 0;
    for (int i = 0; i < AUDIO_PERIOD_CHOICE_COUNT; i++) {
        if (audio_period_choices[i] == session_settings.audio_period_frames) {
            idx = i;
            break;
        }
    }

    idx = (idx + direction + AUDIO_PERIOD_CHOICE_COUNT) % AUDIO_PERIOD_CHOICE_COUNT;
    session_settings.audio_period_frames = audio_period_choices[idx];
    audio_bridge_reset_period_floor();

    LOG_INFO(mux_module, "Audio Period changed to %d frames", session_settings.audio_period_frames);
    audio_bridge_apply_sample_rate();
}

void session_settings_cycle_audio_rate_control(const int direction) {
    int idx = 0;
    for (int i = 0; i < AUDIO_RATE_CONTROL_CHOICE_COUNT; i++) {
        if (audio_rate_control_choices[i] == session_settings.audio_rate_control) {
            idx = i;
            break;
        }
    }

    idx = (idx + direction + AUDIO_RATE_CONTROL_CHOICE_COUNT) % AUDIO_RATE_CONTROL_CHOICE_COUNT;
    session_settings.audio_rate_control = audio_rate_control_choices[idx];

    LOG_INFO(
        mux_module, "Audio Rate Control changed to %s",
        session_settings_audio_rate_control_name(session_settings.audio_rate_control)
    );
}

void session_settings_cycle_game_renderer(const int direction) {
    session_settings.game_renderer =
        (session_settings.game_renderer + direction + game_renderer_count) % game_renderer_count;

    LOG_INFO(
        mux_module, "Game Renderer changed to %s", session_settings_game_renderer_name(session_settings.game_renderer)
    );
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

void session_settings_cycle_hotkey_ff_glyph_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_ff_glyph_enabled = !session_settings.hotkey_ff_glyph_enabled;
}

void session_settings_cycle_hotkey_slowmo_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_slowmo_enabled = !session_settings.hotkey_slowmo_enabled;
}

void session_settings_cycle_hotkey_slowmo_glyph_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_slowmo_glyph_enabled = !session_settings.hotkey_slowmo_glyph_enabled;
}

void session_settings_cycle_hotkey_pause_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_pause_enabled = !session_settings.hotkey_pause_enabled;
}

void session_settings_cycle_hotkey_pause_glyph_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_pause_glyph_enabled = !session_settings.hotkey_pause_glyph_enabled;
}

void session_settings_cycle_hotkey_quicksave_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_quicksave_enabled = !session_settings.hotkey_quicksave_enabled;
}

void session_settings_cycle_hotkey_quickload_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_quickload_enabled = !session_settings.hotkey_quickload_enabled;
}

void session_settings_cycle_hotkey_toggle_fps_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_toggle_fps_enabled = !session_settings.hotkey_toggle_fps_enabled;
}

void session_settings_cycle_hotkey_header_toggle_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_header_toggle_enabled = !session_settings.hotkey_header_toggle_enabled;
}

void session_settings_cycle_hotkey_quit_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_quit_enabled = !session_settings.hotkey_quit_enabled;
}

void session_settings_cycle_hotkey_manual_enabled(const int direction) {
    (void) direction;
    session_settings.hotkey_manual_enabled = !session_settings.hotkey_manual_enabled;
}

void session_settings_cycle_auto_save(const int direction) {
    session_settings.auto_save = (session_settings.auto_save + direction + auto_save_count) % auto_save_count;
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

void session_settings_cycle_sram_backup_enabled(const int direction) {
    (void) direction;
    session_settings.sram_backup_enabled = !session_settings.sram_backup_enabled;
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

void session_settings_cycle_overlay_source(const int direction) {
    session_settings.overlay_source =
        (session_settings.overlay_source + direction + overlay_source_count) % overlay_source_count;
    overlay_bridge_apply();
}

void session_settings_cycle_overlay_pattern(const int direction) {
    const int count = overlay_pattern_count();
    session_settings.overlay_pattern = (session_settings.overlay_pattern + direction + count) % count;
    overlay_bridge_apply();
}

void session_settings_cycle_overlay_opacity(const int direction) {
    session_settings.overlay_opacity += direction * 5;
    if (session_settings.overlay_opacity < 0) session_settings.overlay_opacity = 0;
    if (session_settings.overlay_opacity > 100) session_settings.overlay_opacity = 100;
    overlay_bridge_apply();
}

void session_settings_cycle_viewport_offset_x(const int direction) {
    const int max = device.mux.width / 2;
    session_settings.viewport_offset_x += direction * VIEWPORT_OFFSET_STEP;
    if (session_settings.viewport_offset_x < -max) session_settings.viewport_offset_x = -max;
    if (session_settings.viewport_offset_x > max) session_settings.viewport_offset_x = max;
    video_bridge_apply_scaling();
}

void session_settings_cycle_viewport_offset_y(const int direction) {
    const int max = device.mux.height / 2;
    session_settings.viewport_offset_y += direction * VIEWPORT_OFFSET_STEP;
    if (session_settings.viewport_offset_y < -max) session_settings.viewport_offset_y = -max;
    if (session_settings.viewport_offset_y > max) session_settings.viewport_offset_y = max;
    video_bridge_apply_scaling();
}

void session_settings_cycle_viewport_stretch_x(const int direction) {
    const int max = device.mux.width / 2;
    session_settings.viewport_stretch_x += direction * VIEWPORT_STRETCH_STEP;
    if (session_settings.viewport_stretch_x < -max) session_settings.viewport_stretch_x = -max;
    if (session_settings.viewport_stretch_x > max) session_settings.viewport_stretch_x = max;
    video_bridge_apply_scaling();
}

void session_settings_cycle_viewport_stretch_y(const int direction) {
    const int max = device.mux.height / 2;
    session_settings.viewport_stretch_y += direction * VIEWPORT_STRETCH_STEP;
    if (session_settings.viewport_stretch_y < -max) session_settings.viewport_stretch_y = -max;
    if (session_settings.viewport_stretch_y > max) session_settings.viewport_stretch_y = max;
    video_bridge_apply_scaling();
}

void session_settings_cycle_viewport_zoom(const int direction) {
    session_settings.viewport_zoom += direction * VIEWPORT_ZOOM_STEP;
    if (session_settings.viewport_zoom < VIEWPORT_ZOOM_MIN) session_settings.viewport_zoom = VIEWPORT_ZOOM_MIN;
    if (session_settings.viewport_zoom > VIEWPORT_ZOOM_MAX) session_settings.viewport_zoom = VIEWPORT_ZOOM_MAX;
    video_bridge_apply_scaling();
}

static int viewport_crop_limit(const int frame_dim, const int opposite) {
    int max = VIEWPORT_CROP_MAX;
    if (frame_dim > 0 && frame_dim - opposite - VIEWPORT_CROP_MIN_KEEP < max)
        max = frame_dim - opposite - VIEWPORT_CROP_MIN_KEEP;
    if (max < 0) max = 0;
    return max;
}

static void cycle_viewport_crop(int *value, const int direction, const int frame_dim, const int opposite) {
    const int max = viewport_crop_limit(frame_dim, opposite);
    *value += direction * VIEWPORT_CROP_STEP;
    if (*value < 0) *value = 0;
    if (*value > max) *value = max;
    video_bridge_apply_scaling();
}

void session_settings_cycle_viewport_crop_top(const int direction) {
    int frame_w, frame_h;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    cycle_viewport_crop(&session_settings.viewport_crop_top, direction, frame_h, session_settings.viewport_crop_bottom);
}

void session_settings_cycle_viewport_crop_bottom(const int direction) {
    int frame_w, frame_h;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    cycle_viewport_crop(&session_settings.viewport_crop_bottom, direction, frame_h, session_settings.viewport_crop_top);
}

void session_settings_cycle_viewport_crop_left(const int direction) {
    int frame_w, frame_h;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    cycle_viewport_crop(&session_settings.viewport_crop_left, direction, frame_w, session_settings.viewport_crop_right);
}

void session_settings_cycle_viewport_crop_right(const int direction) {
    int frame_w, frame_h;
    video_bridge_get_frame_size(&frame_w, &frame_h);
    cycle_viewport_crop(&session_settings.viewport_crop_right, direction, frame_w, session_settings.viewport_crop_left);
}

void session_settings_cycle_viewport_centre_crop(const int direction) {
    (void) direction;
    session_settings.viewport_centre_crop = !session_settings.viewport_centre_crop;
    video_bridge_apply_scaling();
}

void session_settings_cycle_frame_delay(const int direction) {
    int idx = 0;
    for (int i = 0; i < FRAME_DELAY_CHOICE_COUNT; i++) {
        if (frame_delay_choices[i] == session_settings.frame_delay_ms) {
            idx = i;
            break;
        }
    }

    idx = (idx + direction + FRAME_DELAY_CHOICE_COUNT) % FRAME_DELAY_CHOICE_COUNT;
    session_settings.frame_delay_ms = frame_delay_choices[idx];
}

void session_settings_cycle_stick_deadzone(const int direction) {
    session_settings.stick_deadzone += direction * STICK_STEP;
    if (session_settings.stick_deadzone < STICK_DEADZONE_MIN) session_settings.stick_deadzone = STICK_DEADZONE_MIN;
    if (session_settings.stick_deadzone > STICK_DEADZONE_MAX) session_settings.stick_deadzone = STICK_DEADZONE_MAX;
}

void session_settings_cycle_stick_anti_deadzone(const int direction) {
    session_settings.stick_anti_deadzone += direction * STICK_STEP;
    if (session_settings.stick_anti_deadzone < STICK_ANTI_DEADZONE_MIN)
        session_settings.stick_anti_deadzone = STICK_ANTI_DEADZONE_MIN;
    if (session_settings.stick_anti_deadzone > STICK_ANTI_DEADZONE_MAX)
        session_settings.stick_anti_deadzone = STICK_ANTI_DEADZONE_MAX;
}

void session_settings_cycle_stick_sensitivity(const int direction) {
    session_settings.stick_sensitivity += direction * STICK_STEP;
    if (session_settings.stick_sensitivity < STICK_SENSITIVITY_MIN)
        session_settings.stick_sensitivity = STICK_SENSITIVITY_MIN;
    if (session_settings.stick_sensitivity > STICK_SENSITIVITY_MAX)
        session_settings.stick_sensitivity = STICK_SENSITIVITY_MAX;
}

void session_settings_cycle_stick_invert_y(const int direction) {
    (void) direction;
    session_settings.stick_invert_y = !session_settings.stick_invert_y;
}

void session_settings_cycle_audio_latency(const int direction) {
    session_settings.audio_latency_profile =
        (session_settings.audio_latency_profile + direction + audio_latency_count) % audio_latency_count;
}

void session_settings_cycle_shimmer_fix(const int direction) {
    (void) direction;
    session_settings.shimmer_fix = !session_settings.shimmer_fix;
    video_bridge_apply_scaling();
}

void session_settings_cycle_run_ahead(const int direction) {
    (void) direction;
    session_settings.run_ahead = !session_settings.run_ahead;
}

void session_settings_cycle_gpu_hard_sync(const int direction) {
    (void) direction;
    session_settings.gpu_hard_sync = !session_settings.gpu_hard_sync;
}

static int port_claimed_elsewhere(const char *stable_key, const int for_port) {
    for (int p = 0; p < MUX_INPUT_PORT_COUNT; p++) {
        if (p == for_port) continue;
        if (session_settings.port_assignment[p] != port_assignment_remembered) continue;
        if (strcmp(session_settings.port_device_key[p], stable_key) == 0) return 1;
    }
    return 0;
}

static int port_choice_count(int *modes, char keys[][64], const int max_choices, const int for_port) {
    int count = 0;

    modes[count] = port_assignment_auto;
    keys[count][0] = '\0';
    count++;

    for (int s = 0; s < mux_input_source_count() && count < max_choices - 1; s++) {
        mux_input_source_info info;
        if (!mux_input_source_get(s, &info) || !info.connected) continue;
        if (port_claimed_elsewhere(info.stable_key, for_port)) continue;

        modes[count] = port_assignment_remembered;
        snprintf(keys[count], 64, "%s", info.stable_key);
        count++;
    }

    modes[count] = port_assignment_none;
    keys[count][0] = '\0';
    count++;

    return count;
}

void session_settings_cycle_port_controller(const int port, const int direction) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;

    int modes[2 + MUX_INPUT_PORT_COUNT];
    char keys[2 + MUX_INPUT_PORT_COUNT][64];
    const int count = port_choice_count(modes, keys, 2 + MUX_INPUT_PORT_COUNT, port);

    int current = 0;
    for (int i = 0; i < count; i++) {
        if (modes[i] != session_settings.port_assignment[port]) continue;
        if (modes[i] != port_assignment_remembered || strcmp(keys[i], session_settings.port_device_key[port]) == 0) {
            current = i;
            break;
        }
    }

    int next = current + (direction > 0 ? 1 : -1);
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;

    session_settings.port_assignment[port] = modes[next];
    snprintf(session_settings.port_device_key[port], sizeof(session_settings.port_device_key[port]), "%s", keys[next]);
}

void session_settings_port_summary(const int port, char *buf, const size_t len) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) {
        if (len) buf[0] = '\0';
        return;
    }

    const int fell_back = session_settings_resolve_port_source(port) == 0;

    switch (session_settings.port_assignment[port]) {
        case port_assignment_none:
            snprintf(
                buf, len, "%s",
                fell_back ? lang.muxretro.settings_screen.built_in_controls : lang.muxretro.settings_screen.port_none
            );
            return;

        case port_assignment_remembered:
            for (int s = 0; s < mux_input_source_count(); s++) {
                mux_input_source_info info;
                if (!mux_input_source_get(s, &info)) continue;
                if (strcmp(info.stable_key, session_settings.port_device_key[port]) != 0) continue;
                if (!info.connected) break;

                snprintf(buf, len, "%s", info.is_builtin ? lang.muxretro.settings_screen.built_in_controls : info.name);
                return;
            }

            snprintf(
                buf, len, "%s", fell_back ? lang.muxretro.settings_screen.built_in_controls : lang.generic.not_connected
            );
            return;

        default:
            snprintf(buf, len, "%s", lang.muxretro.settings_screen.port_auto);
    }
}

#define PORT_DEVICE_CHOICE_MAX 17

void session_settings_cycle_port_device(const int port, const int direction) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;

    unsigned ids[PORT_DEVICE_CHOICE_MAX];
    int count = 0;

    ids[count++] = 0; // "Default (Joypad)"

    const int type_count = core_input_meta_port_type_count(port);
    for (int i = 0; i < type_count && count < PORT_DEVICE_CHOICE_MAX; i++) {
        unsigned id;
        if (!core_input_meta_port_type_get(port, i, NULL, 0, &id)) continue;

        const unsigned masked = id & RETRO_DEVICE_MASK;
        if (masked != RETRO_DEVICE_JOYPAD && masked != RETRO_DEVICE_ANALOG) continue;

        ids[count++] = id;
    }

    int current = 0;
    for (int i = 0; i < count; i++) {
        if (ids[i] == (unsigned) session_settings.port_device_id[port]) {
            current = i;
            break;
        }
    }

    int next = current + (direction > 0 ? 1 : -1);
    if (next < 0) next = count - 1;
    if (next >= count) next = 0;

    session_settings.port_device_id[port] = (int) ids[next];
    input_bridge_apply_controller_ports();
}

void session_settings_port_device_summary(const int port, char *buf, const size_t len) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) {
        if (len) buf[0] = '\0';
        return;
    }

    const int id = session_settings.port_device_id[port];
    if (id == 0) {
        snprintf(buf, len, "%s", lang.muxretro.settings_screen.core_device_default);
        return;
    }

    const int type_count = core_input_meta_port_type_count(port);
    for (int i = 0; i < type_count; i++) {
        char desc[64];
        unsigned type_id;
        if (!core_input_meta_port_type_get(port, i, desc, sizeof(desc), &type_id)) continue;
        if ((int) type_id == id) {
            snprintf(buf, len, "%s", desc);
            return;
        }
    }

    snprintf(buf, len, "#%d", id);
}

void session_settings_resolve_port_sources(int *resolved) {
    if (!resolved) return;

    int claimed[MUX_INPUT_PORT_COUNT] = {0}; // indexed by mux_input_source index, not port index!

    for (int p = 0; p < MUX_INPUT_PORT_COUNT; p++) {
        resolved[p] = -1;
        if (session_settings.port_assignment[p] != port_assignment_remembered) continue;

        for (int s = 0; s < mux_input_source_count(); s++) {
            mux_input_source_info info;
            if (!mux_input_source_get(s, &info) || !info.connected || claimed[s]) continue;

            if (strcmp(info.stable_key, session_settings.port_device_key[p]) == 0) {
                resolved[p] = s;
                claimed[s] = 1;
                break;
            }
        }
    }

    // port one keeps the built-in controls unless another port was told to use them!
    if (!claimed[0] && resolved[0] < 0) {
        mux_input_source_info builtin;
        if (mux_input_source_get(0, &builtin) && builtin.connected) {
            resolved[0] = 0;
            claimed[0] = 1;
        }
    }

    for (int p = 0; p < MUX_INPUT_PORT_COUNT; p++) {
        if (resolved[p] >= 0 || session_settings.port_assignment[p] != port_assignment_auto) continue;

        for (int s = 0; s < mux_input_source_count(); s++) {
            mux_input_source_info info;
            if (!mux_input_source_get(s, &info) || !info.connected || claimed[s]) continue;

            resolved[p] = s;
            claimed[s] = 1;
            break;
        }
    }
}

int session_settings_resolve_port_source(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return -1;

    int resolved[MUX_INPUT_PORT_COUNT];
    session_settings_resolve_port_sources(resolved);

    return resolved[port];
}

const char *session_settings_button_type_label(const int type) {
    switch (type) {
        case mux_input_b:
            return lang.muxretro.settings_screen.target_b;
        case mux_input_y:
            return lang.muxretro.settings_screen.target_y;
        case mux_input_select:
            return lang.muxretro.settings_screen.target_select;
        case mux_input_start:
            return lang.muxretro.settings_screen.target_start;
        case mux_input_dpad_up:
            return lang.muxretro.settings_screen.target_dpad_up;
        case mux_input_dpad_down:
            return lang.muxretro.settings_screen.target_dpad_down;
        case mux_input_dpad_left:
            return lang.muxretro.settings_screen.target_dpad_left;
        case mux_input_dpad_right:
            return lang.muxretro.settings_screen.target_dpad_right;
        case mux_input_a:
            return lang.muxretro.settings_screen.target_a;
        case mux_input_x:
            return lang.muxretro.settings_screen.target_x;
        case mux_input_l1:
            return lang.muxretro.settings_screen.target_l1;
        case mux_input_r1:
            return lang.muxretro.settings_screen.target_r1;
        case mux_input_l2:
            return lang.muxretro.settings_screen.target_l2;
        case mux_input_r2:
            return lang.muxretro.settings_screen.target_r2;
        case mux_input_l3:
            return lang.muxretro.settings_screen.target_l3;
        case mux_input_r3:
            return lang.muxretro.settings_screen.target_r3;
        case mux_input_ls_up:
            return lang.muxretro.settings_screen.stick_ls_up;
        case mux_input_ls_down:
            return lang.muxretro.settings_screen.stick_ls_down;
        case mux_input_ls_left:
            return lang.muxretro.settings_screen.stick_ls_left;
        case mux_input_ls_right:
            return lang.muxretro.settings_screen.stick_ls_right;
        case mux_input_rs_up:
            return lang.muxretro.settings_screen.stick_rs_up;
        case mux_input_rs_down:
            return lang.muxretro.settings_screen.stick_rs_down;
        case mux_input_rs_left:
            return lang.muxretro.settings_screen.stick_rs_left;
        case mux_input_rs_right:
            return lang.muxretro.settings_screen.stick_rs_right;
        default:
            return lang.muxretro.settings_screen.unbound;
    }
}

static const int target_display_order[PORT_TARGET_COUNT] = {8, 0, 9, 1, 10, 11, 12, 13, 14, 15, 2,  3,
                                                            4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23};

int session_settings_target_at_position(const int position) {
    if (position < 0 || position >= PORT_TARGET_COUNT) return -1;
    return target_display_order[position];
}

int session_settings_target_position(const int target_id) {
    for (int i = 0; i < PORT_TARGET_COUNT; i++) {
        if (target_display_order[i] == target_id) return i;
    }

    return -1;
}

static const char *stick_target_labels[PORT_TARGET_COUNT - PORT_DIGITAL_COUNT] = {
    lang.muxretro.settings_screen.stick_ls_up,   lang.muxretro.settings_screen.stick_ls_down,
    lang.muxretro.settings_screen.stick_ls_left, lang.muxretro.settings_screen.stick_ls_right,
    lang.muxretro.settings_screen.stick_rs_up,   lang.muxretro.settings_screen.stick_rs_down,
    lang.muxretro.settings_screen.stick_rs_left, lang.muxretro.settings_screen.stick_rs_right,
};

const char *session_settings_target_label(const int target_id) {
    if (target_id < 0 || target_id >= PORT_TARGET_COUNT) return lang.muxretro.settings_screen.unbound;
    if (target_id >= PORT_DIGITAL_COUNT) return stick_target_labels[target_id - PORT_DIGITAL_COUNT];

    return session_settings_button_type_label(default_button_map[target_id]);
}

int session_settings_mux_type_for_target(const int target_id) {
    if (target_id < 0 || target_id >= PORT_DIGITAL_COUNT) return -1;
    return default_button_map[target_id];
}

int session_settings_target_stick(const int target_id, int *stick, int *axis_x, int *axis_y) {
    if (target_id < PORT_DIGITAL_COUNT || target_id >= PORT_TARGET_COUNT) return 0;

    const int offset = target_id - PORT_DIGITAL_COUNT;
    *stick = offset / 4;
    *axis_x = 0;
    *axis_y = 0;

    switch (offset % 4) {
        case 0:
            *axis_y = -PORT_STICK_FULL;
            break;
        case 1:
            *axis_y = PORT_STICK_FULL;
            break;
        case 2:
            *axis_x = -PORT_STICK_FULL;
            break;
        default:
            *axis_x = PORT_STICK_FULL;
            break;
    }

    return 1;
}

void session_settings_source_value(const int port, const int source, char *buf, const size_t len) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT || len == 0) {
        if (len) buf[0] = '\0';
        return;
    }

    const int macro_index = session_settings.port_source_macro[port][source];
    if (macro_index >= 0) {
        snprintf(buf, len, "%s: %s", lang.muxretro.settings_screen.macro_label, macros_get_name_by_index(macro_index));
        return;
    }

    const int target = session_settings.port_source_target[port][source];
    const int rate = session_settings.port_source_turbo[port][source];

    if (target >= 0 && rate > 0) {
        snprintf(buf, len, "%s (%s)", session_settings_target_label(target), session_settings_turbo_rate_name(rate));
    } else {
        snprintf(buf, len, "%s", session_settings_target_label(target));
    }
}

int session_settings_target_for_button(const int pressed_type) {
    for (int i = 0; i < PORT_DIGITAL_COUNT; i++) {
        if (session_settings_source_types[i] == pressed_type) return default_source_target[i];
    }

    for (int i = PORT_DIGITAL_COUNT; i < PORT_SOURCE_COUNT; i++) {
        if (session_settings_source_types[i] == pressed_type) return i;
    }

    return -1;
}

int session_settings_source_for_input(const int pressed_type) {
    for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
        if (session_settings_source_types[s] == pressed_type) return s;
    }

    return -1;
}

void session_settings_unbind_source(const int port, const int source) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT) return;
    session_settings.port_source_target[port][source] = -1;
    session_settings.port_source_turbo[port][source] = 0;
    session_settings.port_source_macro[port][source] = -1;
}

int session_settings_set_source_target(const int port, const int source, const int target) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT) return 0;
    if (target >= PORT_TARGET_COUNT) return 0;

    if (target < 0) {
        session_settings_unbind_source(port, source);
        return 1;
    }

    session_settings.port_source_target[port][source] = target;
    session_settings.port_source_macro[port][source] = -1;
    return 1;
}

int session_settings_set_source_by_button(const int port, const int source, const int pressed_type) {
    const int target = session_settings_target_for_button(pressed_type);
    if (target < 0) return 0;

    return session_settings_set_source_target(port, source, target);
}

static const int turbo_ms_table[] = {48, 96, 192, 384, 768, 1536, 3072, 6144};
#define TURBO_MS_TABLE_COUNT ((int) (sizeof(turbo_ms_table) / sizeof(turbo_ms_table[0])))

void session_settings_cycle_source_turbo(const int port, const int source, const int direction) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT) return;

    const int current = session_settings.port_source_turbo[port][source];

    int idx = -1;
    for (int i = 0; i < TURBO_MS_TABLE_COUNT; i++) {
        if (turbo_ms_table[i] == current) {
            idx = i;
            break;
        }
    }

    idx += direction > 0 ? 1 : -1;
    if (idx < -1) idx = TURBO_MS_TABLE_COUNT - 1;
    if (idx >= TURBO_MS_TABLE_COUNT) idx = -1;

    session_settings.port_source_turbo[port][source] = idx < 0 ? 0 : turbo_ms_table[idx];
}

void session_settings_reset_source(const int port, const int source) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT) return;
    session_settings.port_source_target[port][source] = default_source_target[source];
    session_settings.port_source_turbo[port][source] = 0;
    session_settings.port_source_macro[port][source] = -1;
}

void session_settings_assign_source_macro(const int port, const int source, const int macro_index) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT || source < 0 || source >= PORT_SOURCE_COUNT) return;

    if (macro_index >= 0) {
        for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
            if (s != source && session_settings.port_source_macro[port][s] == macro_index)
                session_settings_reset_source(port, s);
        }
    }

    session_settings.port_source_macro[port][source] = macro_index;
    session_settings.port_source_target[port][source] = -1;
    session_settings.port_source_turbo[port][source] = 0;
}

void session_settings_clear_macro_references(const int macro_index) {
    if (macro_index < 0) return;

    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
            if (session_settings.port_source_macro[i][s] == macro_index) session_settings.port_source_macro[i][s] = -1;
        }
    }
}

static void reset_button_map(const int port) {
    for (int s = 0; s < PORT_SOURCE_COUNT; s++) {
        session_settings.port_source_target[port][s] = default_source_target[s];
        session_settings.port_source_turbo[port][s] = 0;
        session_settings.port_source_macro[port][s] = -1;
    }
}

const char *session_settings_turbo_rate_name(const int rate) {
    static char buf[16];

    if (rate <= 0) return lang.muxretro.settings_screen.turbo_off;

    if (rate >= 1000 && rate % 1000 == 0) {
        snprintf(buf, sizeof(buf), "%ds", rate / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%dms", rate);
    }

    return buf;
}

void session_settings_reset_input_port(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;

    if (port == 0) {
        session_settings.port_assignment[port] = port_assignment_remembered;
        snprintf(session_settings.port_device_key[port], sizeof(session_settings.port_device_key[port]), "builtin");
    } else {
        session_settings.port_assignment[port] = port_assignment_auto;
        session_settings.port_device_key[port][0] = '\0';
    }

    session_settings.port_device_id[port] = (int) core_input_meta_preferred_device(port);
    reset_button_map(port);
}

void session_settings_auto_assign_controllers(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        session_settings_reset_input_port(i);
}

void session_settings_reset_input(void) {
    session_settings_auto_assign_controllers();

    session_settings.rumble_enabled = defaults.rumble_enabled;
    session_settings.stick_deadzone = defaults.stick_deadzone;
    session_settings.stick_anti_deadzone = defaults.stick_anti_deadzone;
    session_settings.stick_sensitivity = defaults.stick_sensitivity;
    session_settings.stick_invert_y = defaults.stick_invert_y;

    input_bridge_apply_controller_ports();
}

void session_settings_apply_performance_first(void) {
    session_settings.texture_filter = texture_filter_nearest;
    session_settings.shimmer_fix = 0;

    session_settings.colour_filter = 0;
    session_settings.colour_shader = 0;
    session_settings.colour_brightness = defaults.colour_brightness;
    session_settings.colour_contrast = defaults.colour_contrast;
    session_settings.colour_saturation = defaults.colour_saturation;
    session_settings.colour_hueshift = defaults.colour_hueshift;
    session_settings.colour_gamma = defaults.colour_gamma;

    session_settings.overlay_source = overlay_source_off;

    video_bridge_apply_filter();
    video_bridge_apply_scaling();
    colour_refresh();
    overlay_bridge_apply();
}

void session_settings_reset_viewport(void) {
    session_settings.viewport_offset_x = 0;
    session_settings.viewport_offset_y = 0;
    session_settings.viewport_stretch_x = 0;
    session_settings.viewport_stretch_y = 0;
    session_settings.viewport_zoom = 100;
    session_settings.viewport_crop_top = 0;
    session_settings.viewport_crop_bottom = 0;
    session_settings.viewport_crop_left = 0;
    session_settings.viewport_crop_right = 0;
    session_settings.viewport_centre_crop = 0;
    video_bridge_apply_scaling();
}

int session_settings_is_dirty(void) {
    return memcmp(&session_settings, &baseline_settings, sizeof(session_settings)) != 0;
}

void session_settings_apply_save_choice(const int choice) {
    switch (choice) {
        case 0:
            session_settings_save_content();
            break;
        case 1:
            session_settings_save_core();
            break;
        case 2:
            session_settings_save_directory();
            break;
        default:
            session_settings_discard();
            break;
    }
}

void session_settings_discard_to(const struct session_settings_t *snapshot) {
    session_settings = *snapshot;
    video_bridge_apply_scaling();
    video_bridge_apply_filter();
    session_settings_apply_fps_mode();
    audio_bridge_apply_sample_rate();
    video_bridge_apply_fps_limit();
    colour_refresh();
    overlay_bridge_apply();
}

void session_settings_discard(void) {
    session_settings_discard_to(&baseline_settings);
}

void session_settings_save_content(void) {
    const struct session_settings_t base = tier_base(1, 1);
    write_ini_delta(content_ini_path, &base);
    baseline_settings = session_settings;
}

void session_settings_save_core(void) {
    const struct session_settings_t base = tier_base(0, 0);
    write_ini_delta(core_ini_path, &base);
    baseline_settings = session_settings;
}

void session_settings_save_directory(void) {
    const struct session_settings_t base = tier_base(1, 0);
    write_ini_delta(directory_ini_path, &base);
    baseline_settings = session_settings;
}
