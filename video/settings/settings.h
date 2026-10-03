#pragma once

#include <stddef.h>

typedef enum {
    wasabi_setting_scaling = 0,
    wasabi_setting_rotation,
    wasabi_setting_mirrored,
    wasabi_setting_aspect,
    wasabi_setting_scale,
    wasabi_setting_texture,
    wasabi_setting_border,
    wasabi_setting_vignette,
    wasabi_setting_colour_filter,
    wasabi_setting_shader,
    wasabi_setting_brightness,
    wasabi_setting_contrast,
    wasabi_setting_saturation,
    wasabi_setting_hue,
    wasabi_setting_gamma,
    wasabi_setting_overlay_source,
    wasabi_setting_overlay_pattern,
    wasabi_setting_overlay_image,
    wasabi_setting_overlay_opacity,
    wasabi_setting_overlay_adjustment,
    wasabi_setting_overlay_cropping,
    wasabi_setting_overlay_reset,
    wasabi_setting_viewport_adjustment,
    wasabi_setting_viewport_cropping,
    wasabi_setting_viewport_reset,
    wasabi_setting_playtime,
    wasabi_setting_header,
    wasabi_setting_progress_bar,
    wasabi_setting_visualiser,
    wasabi_setting_artwork_position,
    wasabi_setting_repeat,
    wasabi_setting_shuffle,
    wasabi_setting_volume,
    wasabi_setting_sample_rate,
    wasabi_setting_audio_latency,
    wasabi_setting_audio_period,
    wasabi_setting_audio_filter,
    wasabi_setting_rate_control,
    wasabi_setting_gapless,
    wasabi_setting_crossfade,
    wasabi_setting_tracker_loop,
    wasabi_setting_fast_forward_mode,
    wasabi_setting_fast_forward_speed,
    wasabi_setting_slow_motion_mode,
    wasabi_setting_slow_motion_speed,
    wasabi_setting_hotkey_pause,
    wasabi_setting_hotkey_save_bookmark,
    wasabi_setting_hotkey_load_bookmark,
    wasabi_setting_hotkey_seek_back,
    wasabi_setting_hotkey_seek_forward,
    wasabi_setting_hotkey_seek_back_long,
    wasabi_setting_hotkey_seek_forward_long,
    wasabi_setting_hotkey_header,
    wasabi_setting_hotkey_repeat,
    wasabi_setting_hotkey_shuffle,
    wasabi_setting_hotkey_quit,
    wasabi_setting_hotkey_fast_forward,
    wasabi_setting_hotkey_slow_motion,
    wasabi_setting_thumbnail,
    wasabi_setting_live_quality,
    wasabi_setting_live_buffer,
    wasabi_setting_sleep,
    wasabi_setting_idle_screensaver,
    wasabi_setting_auto_play,
    wasabi_setting_reset,
    wasabi_setting_count
} wasabi_setting;

typedef enum {
    wasabi_page_root = 0,
    wasabi_page_vignette,
    wasabi_page_overlay_adjustment,
    wasabi_page_overlay_cropping,
    wasabi_page_viewport_adjustment,
    wasabi_page_viewport_cropping,
    wasabi_page_hotkeys,
    wasabi_page_shader_parameters,
    wasabi_page_colour_filter,
    wasabi_page_shader,
    wasabi_page_overlay_image
} wasabi_settings_page;

typedef struct {
    const char *label;
    int first;
    int count;
} wasabi_setting_section;

const char *wasabi_setting_label(wasabi_setting setting);
const char *wasabi_setting_glyph(wasabi_setting setting);
void wasabi_setting_value(wasabi_setting setting, char *value, size_t size);
int wasabi_setting_can_change(wasabi_setting setting);
int wasabi_setting_is_action(wasabi_setting setting);
int wasabi_setting_cycle(wasabi_setting setting, int direction);
int wasabi_setting_reset_viewport(void);
int wasabi_setting_reset_overlay(void);
const wasabi_setting_section *wasabi_setting_sections(size_t *count);
void wasabi_settings_set_audio(int active);
int wasabi_settings_audio_active(void);

const char *wasabi_page_title(wasabi_settings_page page);
int wasabi_page_row_count(wasabi_settings_page page);
const char *wasabi_page_label(wasabi_settings_page page, int row);
const char *wasabi_page_glyph(wasabi_settings_page page, int row);
void wasabi_page_value(wasabi_settings_page page, int row, char *value, size_t size);
int wasabi_page_cycle(wasabi_settings_page page, int row, int direction);
void wasabi_hotkey_set_defaults(void);
void wasabi_hotkey_reset(void);
int wasabi_hotkey_button_valid(int input);
int wasabi_hotkey_uses_menu(wasabi_setting setting);
int wasabi_vignette_secret(int shape);
