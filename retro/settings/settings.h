#pragma once

enum video_scale_mode {
    video_scale_aspect = 0,
    video_scale_integer,
    video_scale_stretch,
    video_scale_full_height,
    video_scale_full_width,
    video_scale_fit,
    video_scale_count
};

enum video_rotate_mode { video_rotate_0 = 0, video_rotate_90, video_rotate_180, video_rotate_270, video_rotate_count };

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

#define FRAME_DELAY_OFF  (-1)
#define FRAME_DELAY_AUTO (-2)

enum audio_latency_mode { audio_latency_low = 0, audio_latency_balanced, audio_latency_compat, audio_latency_count };

enum header_visibility_mode {
    header_visibility_none = 0,
    header_visibility_clock,
    header_visibility_battery,
    header_visibility_both,
    header_visibility_count
};

enum ff_speed_mode { ff_speed_2_x = 0, ff_speed_3_x, ff_speed_4_x, ff_speed_8_x, ff_speed_count };

enum slowmo_speed_mode { slowmo_speed_1_2_x = 0, slowmo_speed_1_4_x, slowmo_speed_1_8_x, slowmo_speed_count };

enum overlay_source_mode {
    overlay_source_off = 0,
    overlay_source_pattern,
    overlay_source_catalogue,
    overlay_source_count
};

enum auto_save_mode { auto_save_off = 0, auto_save_idle, auto_save_quit, auto_save_idle_quit, auto_save_count };

struct session_settings_t {
    int scaling_mode;
    int rotate;
    int mirrored;
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
    int hotkey_ff_glyph_enabled;
    int hotkey_slowmo_enabled;
    int hotkey_slowmo_glyph_enabled;
    int hotkey_quicksave_enabled;
    int hotkey_quickload_enabled;
    int hotkey_toggle_fps_enabled;
    int hotkey_header_toggle_enabled;
    int hotkey_quit_enabled;
    int hotkey_analog_toggle_enabled;
    int auto_save;
    int sram_flush_seconds;
    int colour_brightness;
    int colour_contrast;
    int colour_saturation;
    int colour_hueshift;
    int colour_gamma;
    int colour_filter;
    int colour_shader;
    int overlay_source;
    int overlay_pattern;
    int overlay_opacity;
    int viewport_offset_x;
    int viewport_offset_y;
    int viewport_zoom;
    int frame_delay_ms;
    int analog_deadzone;
    int analog_anti_deadzone;
    int analog_sensitivity;
    int analog_invert_y;
    int audio_latency_profile;
    int shimmer_fix;
    int run_ahead;
    int analog_controller;
};

extern struct session_settings_t session_settings;

const char *session_settings_scale_name(int mode);

const char *session_settings_rotate_name(int mode);

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

const char *session_settings_auto_save_name(int mode);

int session_settings_auto_save_on_idle(void);

int session_settings_auto_save_on_quit(void);

const char *session_settings_colour_brightness_name(int value);

const char *session_settings_colour_contrast_name(int value);

const char *session_settings_colour_saturation_name(int value);

const char *session_settings_colour_hueshift_name(int value);

const char *session_settings_colour_gamma_name(int value);

const char *session_settings_colour_filter_name(int index);

const char *session_settings_colour_shader_name(int index);

const char *session_settings_overlay_source_name(int mode);

const char *session_settings_overlay_pattern_name(int index);

const char *session_settings_overlay_opacity_name(int value);

const char *session_settings_viewport_offset_x_name(int value);

const char *session_settings_viewport_offset_y_name(int value);

const char *session_settings_viewport_zoom_name(int value);

const char *session_settings_frame_delay_name(int value);

const char *session_settings_analog_deadzone_name(int value);

const char *session_settings_analog_anti_deadzone_name(int value);

const char *session_settings_analog_sensitivity_name(int value);

const char *session_settings_audio_latency_name(int mode);

void session_settings_init(const char *core_path_arg, const char *content_path);

void session_settings_cycle_scaling(int direction);

void session_settings_cycle_rotate(int direction);

void session_settings_cycle_mirrored(int direction);

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

void session_settings_cycle_hotkey_ff_glyph_enabled(int direction);

void session_settings_cycle_hotkey_slowmo_enabled(int direction);

void session_settings_cycle_hotkey_slowmo_glyph_enabled(int direction);

void session_settings_cycle_hotkey_quicksave_enabled(int direction);

void session_settings_cycle_hotkey_quickload_enabled(int direction);

void session_settings_cycle_hotkey_toggle_fps_enabled(int direction);

void session_settings_cycle_hotkey_header_toggle_enabled(int direction);

void session_settings_cycle_hotkey_quit_enabled(int direction);

void session_settings_cycle_hotkey_analog_toggle_enabled(int direction);

void session_settings_cycle_auto_save(int direction);

void session_settings_cycle_sram_flush(int direction);

void session_settings_cycle_colour_brightness(int direction);

void session_settings_cycle_colour_contrast(int direction);

void session_settings_cycle_colour_saturation(int direction);

void session_settings_cycle_colour_hueshift(int direction);

void session_settings_cycle_colour_gamma(int direction);

void session_settings_cycle_colour_filter(int direction);

void session_settings_set_colour_filter(int index);

void session_settings_set_colour_shader(int index);

void session_settings_cycle_overlay_source(int direction);

void session_settings_cycle_overlay_pattern(int direction);

void session_settings_cycle_overlay_opacity(int direction);

void session_settings_cycle_viewport_offset_x(int direction);

void session_settings_cycle_viewport_offset_y(int direction);

void session_settings_cycle_viewport_zoom(int direction);

void session_settings_cycle_frame_delay(int direction);

void session_settings_cycle_analog_deadzone(int direction);

void session_settings_cycle_analog_anti_deadzone(int direction);

void session_settings_cycle_analog_sensitivity(int direction);

void session_settings_cycle_analog_invert_y(int direction);

void session_settings_cycle_audio_latency(int direction);

void session_settings_cycle_shimmer_fix(int direction);

void session_settings_cycle_run_ahead(int direction);

void session_settings_cycle_analog_controller(int direction);

void session_settings_reset_viewport(void);

int session_settings_is_dirty(void);

void session_settings_apply_save_choice(int choice);

void session_settings_discard(void);

void session_settings_save_content(void);

void session_settings_save_core(void);

void session_settings_save_directory(void);
