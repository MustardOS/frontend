#pragma once

enum video_scale_mode { video_scale_aspect = 0, video_scale_integer, video_scale_stretch, video_scale_count };

enum integer_scale_mode {
    integer_scale_auto = 0,
    integer_scale_1_00,
    integer_scale_1_25,
    integer_scale_1_50,
    integer_scale_1_75,
    integer_scale_2_00,
    integer_scale_2_25,
    integer_scale_2_50,
    integer_scale_2_75,
    integer_scale_3_00,
    integer_scale_3_25,
    integer_scale_3_50,
    integer_scale_3_75,
    integer_scale_4_00,
    integer_scale_count
};

enum aspect_ratio_mode {
    aspect_ratio_auto = 0,
    aspect_ratio_4_3,
    aspect_ratio_8_7,
    aspect_ratio_16_9,
    aspect_ratio_16_10,
    aspect_ratio_pixel_perfect,
    aspect_ratio_count
};

enum texture_filter_mode {
    texture_filter_nearest = 0,
    texture_filter_smooth,
    texture_filter_scale2_x,
    texture_filter_scale3_x,
    texture_filter_sharp_bilinear,
    texture_filter_count
};

enum border_color_mode {
    border_color_theme = 0,
    border_color_black,
    border_color_dark_grey,
    border_color_white,
    border_color_count
};

enum fps_limit_mode { fps_limit_60 = 0, fps_limit_50, fps_limit_none, fps_limit_count };

enum header_visibility_mode {
    header_visibility_none = 0,
    header_visibility_clock,
    header_visibility_battery,
    header_visibility_both,
    header_visibility_count
};

enum ff_speed_mode { ff_speed_2_x = 0, ff_speed_3_x, ff_speed_4_x, ff_speed_8_x, ff_speed_count };

enum slowmo_speed_mode { slowmo_speed_1_2_x = 0, slowmo_speed_1_4_x, slowmo_speed_1_8_x, slowmo_speed_count };

struct session_settings_t {
    int scaling_mode;
    int aspect_ratio;
    int integer_scale;
    int texture_filter;
    int rumble_enabled;
    int volume;
    int show_fps;
    int border_color;
    int sample_rate;
    int fps_limit;
    int header_visibility;
    int ff_speed;
    int slowmo_speed;
    int hotkey_ff_enabled;
    int hotkey_slowmo_enabled;
    int hotkey_quicksave_enabled;
    int hotkey_quickload_enabled;
    int sram_flush_seconds;
    int colour_brightness;
    int colour_contrast;
    int colour_saturation;
    int colour_hueshift;
    int colour_gamma;
    int colour_filter;
    int colour_shader;
};

extern struct session_settings_t session_settings;

const char *session_settings_scale_name(int mode);

const char *session_settings_aspect_ratio_name(int mode);

const char *session_settings_integer_scale_name(int mode);

double session_settings_integer_scale_value(int mode);

const char *session_settings_filter_name(int mode);

const char *session_settings_border_name(int mode);

const char *session_settings_sample_rate_name(int rate);

const char *session_settings_fps_limit_name(int mode);

const char *session_settings_header_visibility_name(int mode);

const char *session_settings_ff_speed_name(int mode);

const char *session_settings_slowmo_speed_name(int mode);

double session_settings_ff_speed_value(int mode);

double session_settings_slowmo_speed_value(int mode);

const char *session_settings_sram_flush_name(int seconds);

const char *session_settings_colour_brightness_name(int value);

const char *session_settings_colour_contrast_name(int value);

const char *session_settings_colour_saturation_name(int value);

const char *session_settings_colour_hueshift_name(int value);

const char *session_settings_colour_gamma_name(int value);

const char *session_settings_colour_filter_name(int index);

const char *session_settings_colour_shader_name(int index);

void session_settings_init(const char *core_path_arg, const char *content_path);

void session_settings_cycle_scaling(int direction);

void session_settings_cycle_aspect_ratio(int direction);

void session_settings_cycle_integer_scale(int direction);

void session_settings_cycle_filter(int direction);

void session_settings_cycle_rumble(int direction);

void session_settings_cycle_volume(int direction);

void session_settings_cycle_fps(int direction);

void session_settings_cycle_border(int direction);

void session_settings_cycle_sample_rate(int direction);

void session_settings_cycle_fps_limit(int direction);

void session_settings_cycle_header_visibility(int direction);

void session_settings_cycle_ff_speed(int direction);

void session_settings_cycle_slowmo_speed(int direction);

void session_settings_cycle_hotkey_ff_enabled(int direction);

void session_settings_cycle_hotkey_slowmo_enabled(int direction);

void session_settings_cycle_hotkey_quicksave_enabled(int direction);

void session_settings_cycle_hotkey_quickload_enabled(int direction);

void session_settings_cycle_sram_flush(int direction);

void session_settings_cycle_colour_brightness(int direction);

void session_settings_cycle_colour_contrast(int direction);

void session_settings_cycle_colour_saturation(int direction);

void session_settings_cycle_colour_hueshift(int direction);

void session_settings_cycle_colour_gamma(int direction);

void session_settings_cycle_colour_filter(int direction);

void session_settings_set_colour_filter(int index);

void session_settings_set_colour_shader(int index);

int session_settings_is_dirty(void);

void session_settings_discard(void);

void session_settings_save_content(void);

void session_settings_save_core(void);

void session_settings_save_directory(void);
