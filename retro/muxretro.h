#pragma once

#include <stdint.h>
#include "libretro.h"

bool mux_retro_environment_cb(unsigned cmd, void *data);

enum retro_pixel_format mux_retro_get_pixel_format(void);

int mux_retro_disk_get_num_images(void);

unsigned mux_retro_disk_get_image_index(void);

bool mux_retro_disk_set_image_index(unsigned index);

bool mux_retro_disk_set_eject_state(bool ejected);

bool mux_retro_disk_get_eject_state(void);

bool mux_retro_disk_get_image_label(unsigned index, char *label, size_t len);

void mux_retro_video_refresh_cb(const void *data, unsigned width, unsigned height, size_t pitch);

void video_bridge_init(void);

void video_bridge_shutdown(void);

void video_bridge_apply_scaling(void);

void video_bridge_apply_filter(void);

void video_bridge_apply_fps_limit(void);

void video_bridge_flush_frame(void);

void video_bridge_get_frame_size(int *w, int *h);

void video_bridge_get_dest_size(int *w, int *h);

void mux_retro_audio_sample_cb(int16_t left, int16_t right);

size_t mux_retro_audio_sample_batch_cb(const int16_t *data, size_t frames);

int audio_bridge_open(double core_sample_rate);

void audio_bridge_apply_sample_rate(void);

void audio_bridge_close(void);

void audio_bridge_set_paused(int pause);

int audio_bridge_is_active(void);

uint32_t audio_bridge_queued_ms(void);

void audio_bridge_clear_queued(void);

void audio_bridge_get_info(int *freq, int *channels);

void mux_retro_input_poll_cb(void);

int16_t mux_retro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id);

void input_bridge_suppress_held(void);

int state_save(const char *path);

int state_load(const char *path);

void pause_menu_init(void);

void pause_menu_shutdown(void);

int pause_menu_is_active(void);

void pause_menu_toggle(void);

void pause_menu_rebuild(void);

void pause_menu_show_nav_hints(void);

void pause_menu_fix_nav_order(void);

void pause_menu_sync_input_mask(void);

void pause_menu_focus_options_item(void);

void pause_menu_focus_gamestate_item(void);

void pause_menu_focus_diskcontrol_item(void);

void pause_menu_focus_settings_item(void);

void pause_menu_focus_information_item(void);

void pause_menu_set_fps_visible(int visible);

void pause_menu_set_fps_text(const char *text);

int pause_menu_tick(void);

void options_menu_init(void);

void options_menu_open(void);

int options_menu_is_active(void);

void options_menu_tick(void);

void gamestate_menu_init(void);

void gamestate_menu_open(void);

int gamestate_menu_is_active(void);

void gamestate_menu_tick(void);

void diskcontrol_menu_open(void);

int diskcontrol_menu_is_active(void);

void diskcontrol_menu_tick(void);

void settings_menu_init(void);

void settings_menu_open(void);

int settings_menu_is_active(void);

void settings_menu_tick(void);

void information_menu_open(void);

int information_menu_is_active(void);

void information_menu_tick(void);
