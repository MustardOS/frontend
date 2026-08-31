#pragma once

#include <stddef.h>

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
    integer_scale_1,
    integer_scale_2,
    integer_scale_3,
    integer_scale_4,
    integer_scale_5,
    integer_scale_6,
    integer_scale_7,
    integer_scale_8,
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
    texture_filter_scale2_x_smooth,
    texture_filter_super_eagle,
    texture_filter_count
};

enum game_renderer_mode { game_renderer_hardware = 0, game_renderer_software, game_renderer_count };

enum audio_filter_mode { audio_filter_none = 0, audio_filter_low_pass, audio_filter_high_pass, audio_filter_count };

enum border_colour_mode {
    border_colour_theme = 0,
    border_colour_black,
    border_colour_dark_grey,
    border_colour_white,
    border_colour_count
};

enum fps_limit_mode { fps_limit_auto = 0, fps_limit_50, fps_limit_none, fps_limit_count };

#define FRAME_DELAY_OFF  (-1)
#define FRAME_DELAY_AUTO (-2)

enum audio_latency_mode { audio_latency_low = 0, audio_latency_balanced, audio_latency_compat, audio_latency_count };

#define SESSION_VOLUME_MAX 200

enum play_profile {
    play_profile_safe = 0,
    play_profile_balanced,
    play_profile_quality,
    play_profile_count,
    play_profile_unmatched = -1
};

#define SESSION_USER_PROFILE_LIMIT           24
#define SESSION_USER_PROFILE_NAME_MAX        64
#define SESSION_USER_PROFILE_DESCRIPTION_MAX 1024

enum user_profile_scope {
    user_profile_scope_content = 0,
    user_profile_scope_core,
    user_profile_scope_all,
    user_profile_scope_count
};

enum content_precache_mode {
    content_precache_off = 0,
    content_precache_64,
    content_precache_128,
    content_precache_256,
    content_precache_count
};

enum show_fps_mode { show_fps_off = 0, show_fps_simple, show_fps_detailed, show_fps_count };

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

#define MUX_INPUT_PORT_COUNT 4

enum port_assignment_mode { port_assignment_auto = 0, port_assignment_none, port_assignment_remembered };

enum port_role_mode { port_role_player = 0, port_role_ketchup };

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
    int show_playtime;
    int content_precache;
    int border_colour;
    int sample_rate;
    int fps_limit;
    int header_visibility;
    int ff_speed;
    int slowmo_speed;
    int hotkey_ff_enabled;
    int hotkey_ff_glyph_enabled;
    int hotkey_slowmo_enabled;
    int hotkey_slowmo_glyph_enabled;
    int hotkey_pause_enabled;
    int hotkey_pause_glyph_enabled;
    int hotkey_quicksave_enabled;
    int hotkey_quickload_enabled;
    int hotkey_toggle_fps_enabled;
    int hotkey_header_toggle_enabled;
    int hotkey_quit_enabled;
    int hotkey_manual_enabled;
    int auto_save;
    int sram_flush_seconds;
    int timeline_interval;
    int timeline_count;
    int history_depth;
    int trash_count;
    int state_thumbnail;
    int colour_brightness;
    int colour_contrast;
    int colour_saturation;
    int colour_hueshift;
    int colour_gamma;
    int colour_filter;
    int colour_shader;
    int vignette_shape;
    int vignette_scaling;
    int vignette_width;
    int vignette_height;
    int vignette_offset_x;
    int vignette_offset_y;
    int vignette_softness;
    int vignette_strength;
    int vignette_colour;
    int overlay_source;
    int overlay_pattern;
    int overlay_opacity;
    int viewport_offset_x;
    int viewport_offset_y;
    int viewport_stretch_x;
    int viewport_stretch_y;
    int viewport_zoom;
    int viewport_crop_top;
    int viewport_crop_bottom;
    int viewport_crop_left;
    int viewport_crop_right;
    int viewport_centre_crop;
    int frame_delay_ms;
    int stick_deadzone;
    int stick_anti_deadzone;
    int stick_sensitivity;
    int stick_invert_y;
    int audio_latency_profile;
    int audio_period_frames;
    int audio_filter;
    int audio_rate_control;
    int game_renderer;
    int shimmer_fix;
    int run_ahead;
    int gpu_hard_sync;
    int port_assignment[MUX_INPUT_PORT_COUNT];
    char port_device_key[MUX_INPUT_PORT_COUNT][64];
    int port_device_id[MUX_INPUT_PORT_COUNT];
    int port_role[MUX_INPUT_PORT_COUNT];
    int port_deck[MUX_INPUT_PORT_COUNT];
    int port_stick_forced[MUX_INPUT_PORT_COUNT];
    int port_source_target[MUX_INPUT_PORT_COUNT][24];
    int port_source_turbo[MUX_INPUT_PORT_COUNT][24];
    int port_source_macro[MUX_INPUT_PORT_COUNT][24];
};

#define PORT_SOURCE_COUNT  24
#define PORT_DIGITAL_COUNT 16
#define PORT_TARGET_COUNT  24
#define PORT_STICK_FULL    32767

extern const int session_settings_source_types[PORT_SOURCE_COUNT];

extern struct session_settings_t session_settings;

const char *session_settings_scale_name(int mode);

const char *session_settings_rotate_name(int mode);

const char *session_settings_aspect_ratio_name(int mode);

const char *session_settings_integer_scale_name(int mode);

double session_settings_integer_scale_value(int mode);

const char *session_settings_filter_name(int mode);

const char *session_settings_audio_filter_name(int mode);

const char *session_settings_border_name(int mode);

const char *session_settings_sample_rate_name(int rate);

const char *session_settings_fps_limit_name(int mode);

const char *session_settings_show_fps_name(int mode);

void session_settings_cycle_show_playtime(int direction);

const char *session_settings_content_precache_name(int mode);

int session_settings_content_precache_mb(int mode);

const char *session_settings_header_visibility_name(int mode);

const char *session_settings_ff_speed_name(int mode);

const char *session_settings_slowmo_speed_name(int mode);

double session_settings_ff_speed_value(int mode);

double session_settings_slowmo_speed_value(int mode);

const char *session_settings_sram_flush_name(int seconds);

const char *session_settings_timeline_interval_name(int mode);

int session_settings_timeline_interval_ms(void);

void session_settings_cycle_timeline_interval(int direction);

void session_settings_cycle_timeline_count(int direction);

#define VIGNETTE_SIZE_MIN   10
#define VIGNETTE_SIZE_MAX   200
#define VIGNETTE_OFFSET_MIN (-50)
#define VIGNETTE_OFFSET_MAX 50

enum {
    vignette_shape_off = 0,
    vignette_shape_round,
    vignette_shape_square,
    vignette_shape_squircle,
    vignette_shape_cycle_count,
    vignette_shape_flower = vignette_shape_cycle_count,
    vignette_shape_triangle,
    vignette_shape_count
};

enum { vignette_scale_frame = 0, vignette_scale_aspect, vignette_scale_count };

enum { vignette_colour_black = 0, vignette_colour_white, vignette_colour_count };

const char *session_settings_vignette_shape_name(int value);

const char *session_settings_vignette_scaling_name(int value);

const char *session_settings_vignette_colour_name(int value);

const char *session_settings_vignette_size_name(int value);

const char *session_settings_vignette_offset_name(int value);

const char *session_settings_vignette_percent_name(int value);

int session_settings_vignette_active(void);

void session_settings_cycle_vignette_shape(int direction);

void session_settings_cycle_vignette_scaling(int direction);

void session_settings_cycle_vignette_width(int direction);

void session_settings_cycle_vignette_height(int direction);

void session_settings_cycle_vignette_offset_x(int direction);

void session_settings_cycle_vignette_offset_y(int direction);

void session_settings_cycle_vignette_softness(int direction);

void session_settings_cycle_vignette_strength(int direction);

void session_settings_cycle_vignette_colour(int direction);

void session_settings_cycle_history_depth(int direction);

const char *session_settings_trash_count_name(int value);

void session_settings_cycle_trash_count(int direction);

enum { state_thumbnail_small = 0, state_thumbnail_medium, state_thumbnail_large, state_thumbnail_count };

const char *session_settings_state_thumbnail_name(int value);

int session_settings_state_thumbnail_width(void);

void session_settings_cycle_state_thumbnail(int direction);

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

const char *session_settings_viewport_stretch_name(int value);

const char *session_settings_viewport_zoom_name(int value);

const char *session_settings_viewport_crop_name(int value);

const char *session_settings_frame_delay_name(int value);

const char *session_settings_stick_deadzone_name(int value);

const char *session_settings_stick_anti_deadzone_name(int value);

const char *session_settings_stick_sensitivity_name(int value);

const char *session_settings_audio_latency_name(int mode);

const char *session_settings_audio_period_name(int frames);

const char *session_settings_audio_rate_control_name(int hundredths);

const char *session_settings_game_renderer_name(int mode);

enum play_profile session_settings_play_profile(void);

void session_settings_apply_play_profile(enum play_profile profile);

int session_settings_refresh_user_profiles(void);

int session_settings_user_profile_count(void);

const char *session_settings_user_profile_name(int index);

const char *session_settings_user_profile_description(int index);

int session_settings_user_profile_apply(int index);

int session_settings_user_profile_current(void);

int session_settings_user_profile_create(const char *name, enum user_profile_scope scope);

int session_settings_user_profile_delete(int index);

void session_settings_init(const char *core_path_arg, const char *content_path);

void session_settings_launch_begin(void);

void session_settings_launch_ready(void);

int session_settings_launch_recovered(void);

void session_settings_cycle_scaling(int direction);

void session_settings_cycle_rotate(int direction);

void session_settings_cycle_mirrored(int direction);

void session_settings_cycle_aspect_ratio(int direction);

void session_settings_cycle_integer_scale(int direction);

void session_settings_cycle_filter(int direction);

void session_settings_cycle_audio_filter(int direction);

void session_settings_cycle_rumble(int direction);

void session_settings_cycle_volume(int direction);

void session_settings_cycle_fps(int direction);

void session_settings_apply_fps_mode(void);

void session_settings_cycle_content_precache(int direction);

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

void session_settings_cycle_hotkey_pause_enabled(int direction);

void session_settings_cycle_hotkey_pause_glyph_enabled(int direction);

void session_settings_cycle_hotkey_quicksave_enabled(int direction);

void session_settings_cycle_hotkey_quickload_enabled(int direction);

void session_settings_cycle_hotkey_toggle_fps_enabled(int direction);

void session_settings_cycle_hotkey_header_toggle_enabled(int direction);

void session_settings_cycle_hotkey_quit_enabled(int direction);

void session_settings_cycle_hotkey_manual_enabled(int direction);

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

void session_settings_cycle_viewport_stretch_x(int direction);

void session_settings_cycle_viewport_stretch_y(int direction);

void session_settings_cycle_viewport_zoom(int direction);

void session_settings_cycle_viewport_crop_top(int direction);

void session_settings_cycle_viewport_crop_bottom(int direction);

void session_settings_cycle_viewport_crop_left(int direction);

void session_settings_cycle_viewport_crop_right(int direction);

void session_settings_cycle_viewport_centre_crop(int direction);

void session_settings_cycle_frame_delay(int direction);

void session_settings_cycle_stick_deadzone(int direction);

void session_settings_cycle_stick_anti_deadzone(int direction);

void session_settings_cycle_stick_sensitivity(int direction);

void session_settings_cycle_stick_invert_y(int direction);

void session_settings_cycle_audio_latency(int direction);

void session_settings_cycle_audio_period(int direction);

void session_settings_cycle_audio_rate_control(int direction);

void session_settings_cycle_game_renderer(int direction);

void session_settings_cycle_shimmer_fix(int direction);

void session_settings_cycle_run_ahead(int direction);

void session_settings_cycle_gpu_hard_sync(int direction);

void session_settings_cycle_port_controller(int port, int direction);

void session_settings_port_summary(int port, char *buf, size_t len);

void session_settings_cycle_port_device(int port, int direction);

void session_settings_port_device_summary(int port, char *buf, size_t len);

void session_settings_set_port_role(int port, int role);

int session_settings_port_is_deck(int port);

void session_settings_set_port_deck(int port, int deck_index);

void session_settings_cycle_port_deck(int port, int direction);

void session_settings_port_deck_summary(int port, char *buf, size_t len);

void session_settings_cycle_port_deck_route(int port, int direction);

void session_settings_port_deck_route_summary(int port, char *buf, size_t len);

void session_settings_cycle_port_deck_priority(int port, int direction);

void session_settings_port_deck_priority_summary(int port, char *buf, size_t len);

int session_settings_port_deck_priority(int port);

int session_settings_port_deck_position(int port);

void session_settings_clear_deck_references(int deck_index);

int session_settings_port_ketchup_route(int port);

int session_settings_default_source_target(int source);

int *session_settings_source_target(int port);

int *session_settings_source_turbo(int port);

int *session_settings_source_macro(int port);

void session_settings_resolve_port_sources(int *resolved);

int session_settings_resolve_port_source(int port);

const char *session_settings_button_type_label(int type);

const char *session_settings_target_label(int target_id);

int session_settings_mux_type_for_target(int target_id);

int session_settings_target_stick(int target_id, int *stick, int *axis_x, int *axis_y);

int session_settings_target_at_position(int position);

int session_settings_target_position(int target_id);

int session_settings_target_for_button(int pressed_type);

int session_settings_source_for_input(int pressed_type);

int session_settings_set_source_by_button(int port, int source, int pressed_type);

int session_settings_set_source_target(int port, int source, int target);

void session_settings_cycle_stick_dpad(int port, int direction);

void session_settings_stick_dpad_summary(int port, char *buf, size_t len);

int session_settings_stick_forced(int port, int index);

void session_settings_cycle_source_turbo(int port, int source, int direction);

void session_settings_unbind_source(int port, int source);

void session_settings_reset_source(int port, int source);

void session_settings_assign_source_macro(int port, int source, int macro_index);

void session_settings_clear_macro_references(int macro_index);

void session_settings_source_value(int port, int source, char *buf, size_t len);

const char *session_settings_turbo_rate_name(int rate);

void session_settings_reset_input_port(int port);

void session_settings_reset_input(void);

void session_settings_auto_assign_controllers(void);

void session_settings_reset_viewport(void);

int session_settings_is_dirty(void);

void session_settings_apply_save_choice(int choice);

void session_settings_discard(void);

void session_settings_discard_to(const struct session_settings_t *snapshot);

void session_settings_save_content(void);

void session_settings_save_core(void);

void session_settings_save_directory(void);

int session_settings_delete_saved_overrides(void);
