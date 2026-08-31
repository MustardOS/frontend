#pragma once

#include <stdint.h>
#include "../../common/input.h"
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

void video_bridge_set_core_aspect(double aspect_ratio);

void video_bridge_set_geometry(unsigned base_width, unsigned base_height, float aspect_ratio);

void video_bridge_set_core_rotation(int quarter_turns);

void video_bridge_apply_filter(void);

void video_bridge_apply_fps_limit(void);

int video_bridge_get_swap_interval(void);

void video_bridge_flush_frame(void);

void video_bridge_set_frame_skip(int skip);

int video_bridge_get_frame_skip(void);

void video_bridge_get_frame_size(int *w, int *h);

void video_bridge_get_dest_size(int *w, int *h);

void mux_retro_audio_sample_cb(int16_t left, int16_t right);

size_t mux_retro_audio_sample_batch_cb(const int16_t *data, size_t frames);

int audio_bridge_open(double core_sample_rate);

void audio_bridge_apply_sample_rate(void);

void audio_bridge_reset_period_floor(void);

void audio_bridge_reconfigure_rate(double new_core_rate);

void audio_bridge_close(void);

void audio_bridge_set_paused(int pause);

int audio_bridge_is_active(void);

int audio_bridge_is_prefilling(void);

uint32_t audio_bridge_queued_ms(void);

uint32_t audio_bridge_low_water_ms(void);

uint32_t audio_bridge_high_water_ms(void);

uint32_t audio_bridge_prefill_target_ms(void);

uint32_t audio_bridge_burst_reservoir_ms(void);

uint32_t audio_bridge_backpressure_ceiling_ms(void);

void audio_bridge_wait_for_headroom(uint32_t budget_ms);

void audio_bridge_wait_for_cadence(void);

void audio_bridge_recover_cadence(void);

void audio_bridge_note_core_frames(unsigned frames);

uint64_t audio_bridge_dropped_frames(void);

uint64_t audio_bridge_latency_recovery_frames(void);

uint64_t audio_bridge_latency_recovery_count(void);

uint32_t audio_bridge_underrun_count(void);

uint32_t audio_bridge_underrun_event_count(void);

uint64_t audio_bridge_underrun_missing_frames(void);

double audio_bridge_rate_correction_percent(void);

double audio_bridge_rate_limit_percent(void);

int audio_bridge_pickles_burst_recovery_active(void);

uint64_t audio_bridge_pickles_burst_recovery_count(void);

double audio_bridge_pickles_burst_recovery_peak_percent(void);

uint64_t audio_bridge_batch_calls(void);

size_t audio_bridge_batch_peak_frames(void);

double audio_bridge_content_fps(void);

double audio_bridge_locked_content_fps(void);

double audio_bridge_content_quantum_fps(void);

double audio_bridge_core_pace_divisor(void);

void audio_bridge_drc_tick(void);

void audio_bridge_request_min_latency(unsigned ms);

void audio_bridge_set_latency_floor(unsigned ms);

uint32_t audio_bridge_latency_floor_ms(void);

void audio_bridge_apply_pending_min_latency(void);

void audio_bridge_set_buffer_status_callback(retro_audio_buffer_status_callback_t cb);

void audio_bridge_notify_buffer_status(void);

void core_prime_audio(void);

void audio_bridge_clear_queued(void);

void audio_bridge_flush_sample_fifo(void);

void audio_bridge_discard_sample_fifo(void);

void audio_bridge_set_muted(int mute);

int audio_bridge_is_muted(void);

void audio_bridge_get_info(int *freq, int *channels);

unsigned audio_bridge_target_sample_rate(void);

void audio_bridge_reset_content_rate(void);

void audio_bridge_apply_filter(void);

void mux_retro_input_poll_cb(void);

int16_t mux_retro_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id);

void input_bridge_begin_run(void);

uint64_t input_bridge_snapshot_signature(void);

void input_bridge_apply_controller_ports(void);

void input_bridge_set_netplay_state(unsigned player_count, int routes_input);

int environment_core_wants_hw_render(void);

int environment_hw_render_refused(void);

int environment_av_info_pending(void);

void environment_apply_pending_av_info(void);

void environment_notify_frame_time(void);

int environment_frame_time_callback_active(void);

uint32_t environment_frame_time_clamp_count(void);

double environment_frame_time_clamp_peak_ms(void);

void frame_pacer_maybe_wait(void);

void frame_pacer_after_present(void);

void frame_pacer_wait_until(uint64_t deadline_counter);

float frame_pacer_get_refresh_hz(void);

float frame_pacer_get_delay_ms(void);

void core_set_target_fps(double new_fps);

double core_get_target_fps(void);

double core_pace_divisor(void);

int core_content_needs_pacing(void);

double core_auto_pace_target_ms(void);

int core_pacing_uses_audio_clock(void);

void input_bridge_suppress_held(void);

void input_bridge_suppress(mux_input_type type);

void state_saves_init(const char *core_file_path);

int state_saves_supported(void);

int state_saves_warmup_frames(void);

int state_save(const char *path);

int state_load(const char *path, int show_message);

int state_flush(void);

int state_write_poll(int *result);

void state_shutdown(void);

void pause_menu_init(void);

void pause_menu_shutdown(void);

int pause_menu_is_active(void);

void advisory_init(const char *content_path);

void advisory_tick(uint32_t now);

void advisory_shutdown(void);

int pause_menu_peek_allowed(void);

int pause_menu_take_menu_tap(void);

int pause_menu_help_input(int scroll_up, int scroll_down, int dismiss);

void pause_menu_toggle(void);

void pause_menu_rebuild(void);

void pause_menu_show_nav_hints(void);

void pause_menu_fix_nav_order(void);

void pause_menu_sync_input_mask(void);

void settings_menu_reopen_core_options(void);

void pause_menu_focus_gamestate_item(void);

void pause_menu_focus_diskcontrol_item(void);

void pause_menu_focus_cheats_item(void);

void pause_menu_focus_patches_item(void);

void pause_menu_focus_settings_item(void);

void pause_menu_focus_information_item(void);

void pause_menu_focus_netplay_item(void);

void pause_menu_focus_game_link_item(void);

void pause_menu_focus_cheevo_item(void);

void pause_menu_set_fps_visible(int visible);

void pause_menu_refresh_gb_slot(void);

void pause_menu_service_tick(uint32_t now);

void pause_menu_playtime_reset(void);

void pause_menu_set_fps_text(const char *text);

void pause_menu_set_speed_indicator(const char *text, const char *glyph);

void pause_menu_show_toast(const char *msg);

void pause_menu_show_toast_timed(const char *msg, uint32_t duration_ms);

void pause_menu_show_glyph_toast_timed(const char *msg, const char *glyph, uint32_t duration_ms);

void pause_menu_update_header(void);

int pause_menu_gameplay_hud_active(void);

void pause_menu_apply_header_visibility(void);

int pause_menu_capture_clean_screenshot(const char *path, int restore_visibility);

int pause_menu_capture_clean_pixels(uint8_t *pixels, int restore_visibility);

int pause_menu_store_clean_screenshot(const char *path, int restore_visibility);

int pause_menu_tick(void);

void options_menu_init(void);

void options_menu_open(void);

int options_menu_is_active(void);

void options_menu_tick(void);

void gamestate_menu_init(void);

void gamestate_menu_open(void);

int gamestate_menu_is_active(void);

void gamestate_menu_tick(void);

void gamestate_notice_open(void);

int gamestate_notice_is_active(void);

void gamestate_notice_tick(void);

void diskcontrol_menu_open(void);

int diskcontrol_menu_is_active(void);

void diskcontrol_menu_tick(void);

void settings_menu_init(void);

void settings_menu_open(void);

int settings_menu_is_active(void);

void settings_menu_tick(void);

void input_menu_reopen_hotkeys(void);

void settings_menu_reopen_video(void);

void settings_menu_reopen_visuals(void);

void settings_menu_reopen_visuals_at(int local_index);

void settings_menu_reopen_overlay(void);

void settings_menu_reopen_viewport_at(int local_index);

void settings_menu_reopen_sound(void);

void settings_menu_reopen_input(void);

void settings_menu_reopen_input_at(int local_index);

void settings_menu_reopen_performance(void);

void settings_menu_reopen_hud(void);

void settings_menu_reopen_storage(void);

void settings_menu_reopen_cheevo(void);

void settings_menu_reopen_cheevo_at(int local_index);

void video_menu_init(void);

void video_menu_open(void);

int video_menu_is_active(void);

void video_menu_tick(void);

void sound_menu_init(void);

void sound_menu_open(void);

int sound_menu_is_active(void);

void sound_menu_tick(void);

void input_menu_init(void);

void input_menu_open(void);

int input_menu_is_active(void);

void input_menu_tick(void);

void input_menu_reopen_port(int port);

void input_menu_reopen_controller_options(void);

void controller_options_menu_init(void);

void controller_options_menu_open(void);

int controller_options_menu_is_active(void);

void controller_options_menu_tick(void);

void input_port_menu_init_all(void);

void input_port_menu_open(int port);

int input_port_menu_is_active(void);

void input_port_menu_tick(void);

void input_port_menu_reopen_button_mapping(int port);

void input_port_menu_reopen_macros(int port);

void button_mapping_menu_init_all(void);

void button_mapping_menu_open(int port);

int button_mapping_menu_is_active(void);

void button_mapping_menu_tick(void);

void macros_menu_init(void);

void macros_menu_open(int port);

int macros_menu_is_active(void);

void macros_menu_tick(void);

void performance_menu_init(void);

void performance_menu_open(void);

int performance_menu_is_active(void);

void performance_menu_tick(void);

void hud_menu_init(void);

void hud_menu_open(void);

int hud_menu_is_active(void);

void hud_menu_tick(void);

void storage_menu_init(void);

void storage_menu_open(void);

int storage_menu_is_active(void);

void storage_menu_tick(void);

void display_menu_init(void);

void display_menu_open(void);

int display_menu_is_active(void);

void display_menu_tick(void);

void display_menu_reopen_filter(void);

void display_menu_reopen_shader(void);

void overlay_menu_init(void);

void overlay_menu_open(void);

int overlay_menu_is_active(void);

void overlay_menu_tick(void);

void colfilter_menu_init(void);

void colfilter_menu_open(void);

int colfilter_menu_is_active(void);

void colfilter_menu_tick(void);

void vignette_menu_init(void);

void vignette_menu_open(void);

int vignette_menu_is_active(void);

void vignette_menu_tick(void);

void display_menu_reopen_vignette(void);

void shader_menu_init(void);

void shader_menu_open(void);

int shader_menu_is_active(void);

void shader_menu_reopen(void);

void shader_adjust_menu_init(void);

void shader_adjust_menu_open(void);

int shader_adjust_menu_is_active(void);

void shader_adjust_menu_tick(void);

void shader_menu_tick(void);

void viewport_menu_init(void);

void viewport_adjust_menu_init(void);

void viewport_adjust_menu_open(void);

int viewport_adjust_menu_is_active(void);

void viewport_adjust_menu_tick(void);

void viewport_crop_menu_init(void);

void viewport_crop_menu_open(void);

int viewport_crop_menu_is_active(void);

void viewport_crop_menu_tick(void);

void hotkeys_menu_init(void);

void hotkeys_menu_open(void);

int hotkeys_menu_is_active(void);

void hotkeys_menu_tick(void);

void cheats_menu_init(void);

void cheats_menu_open(void);

int cheats_menu_is_active(void);

void cheats_menu_tick(void);

void patch_menu_init(void);

void patch_menu_open(void);

int patch_menu_is_active(void);

void patch_menu_tick(void);

void manual_menu_init(void);

void manual_menu_open(void);

int manual_menu_is_active(void);

void manual_menu_tick(void);

void information_menu_open(void);

int information_menu_is_active(void);

void information_menu_tick(void);
