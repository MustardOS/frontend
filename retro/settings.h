#pragma once

enum video_scale_mode {
    video_scale_aspect = 0,
    video_scale_integer_x1,
    video_scale_integer_x2,
    video_scale_integer_x3,
    video_scale_stretch,
    video_scale_integer_auto,
    video_scale_count
};

enum texture_filter_mode {
    texture_filter_nearest = 0,
    texture_filter_smooth,
    texture_filter_scale2_x,
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

struct session_settings_t {
    int scaling_mode;
    int texture_filter;
    int rumble_enabled;
    int volume;
    int show_fps;
    int border_color;
    int sample_rate;
    int fps_limit;
};

extern struct session_settings_t session_settings;

const char *session_settings_scale_name(int mode);

const char *session_settings_filter_name(int mode);

const char *session_settings_border_name(int mode);

const char *session_settings_sample_rate_name(int rate);

const char *session_settings_fps_limit_name(int mode);

void session_settings_init(const char *core_path_arg, const char *content_path);

void session_settings_cycle_scaling(int direction);

void session_settings_cycle_filter(int direction);

void session_settings_cycle_rumble(int direction);

void session_settings_cycle_volume(int direction);

void session_settings_cycle_fps(int direction);

void session_settings_cycle_border(int direction);

void session_settings_cycle_sample_rate(int direction);

void session_settings_cycle_fps_limit(int direction);

int session_settings_is_dirty(void);

void session_settings_discard(void);

void session_settings_save_content(void);

void session_settings_save_core(void);

void session_settings_save_directory(void);
