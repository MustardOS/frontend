#pragma once

#include <stddef.h>
#include "../content/library.h"

typedef enum {
    video_ui_action_none,
    video_ui_action_opened,
    video_ui_action_closed,
    video_ui_action_load_bookmark,
    video_ui_action_load_playlist,
    video_ui_action_switch_content,
    video_ui_action_restart,
    video_ui_action_stop,
} video_ui_action;

int video_playback_ui_init(
    const char *title, const char *uri, const char *content_uri, int live, const video_library_entry *playlist,
    size_t playlist_count, size_t playlist_index, int playlist_channels
);
void video_playback_ui_shutdown(void);
int video_playback_ui_menu_active(void);
int video_playback_ui_confirmable(void);
int video_playback_ui_y_actionable(void);
int video_playback_ui_modal_active(void);
void video_playback_ui_modal_move(int direction);
void video_playback_ui_modal_confirm(void);
void video_playback_ui_modal_cancel(void);
void video_playback_ui_menu_press(void);
void video_playback_ui_menu_consume(void);
int video_playback_ui_menu_release(void);
video_ui_action video_playback_ui_toggle_menu(void);
video_ui_action video_playback_ui_confirm(void);
video_ui_action video_playback_ui_back(void);
void video_playback_ui_move(int steps, int direction);
void video_playback_ui_move_held(int steps, int direction);
void video_playback_ui_section(int direction);
void video_playback_ui_change(int direction);
void video_playback_ui_tick(void);
void video_playback_ui_set_paused(int paused);
void video_playback_ui_header_changed(void);
void video_playback_ui_modes_changed(void);
void video_playback_ui_update_position(double position, double duration);
void video_playback_ui_show_position(double position, double duration);
void video_playback_ui_show_speed(const char *value, const char *glyph);
void video_playback_ui_show_channel(size_t index);
void video_playback_ui_hide_channel(void);
void video_playback_ui_prepare_channel_transition(void);
int video_playback_ui_bookmarks_active(void);
video_ui_action video_playback_ui_begin_bookmark_name(void);
int video_playback_ui_naming_active(void);
int video_playback_ui_name_press(char *name, size_t name_size);
int video_playback_ui_name_finish(char *name, size_t name_size);
void video_playback_ui_name_cancel(void);
void video_playback_ui_name_backspace(void);
void video_playback_ui_name_clear(void);
void video_playback_ui_name_space(void);
void video_playback_ui_name_move(int vertical, int direction);
void video_playback_ui_name_layer(int direction);
void video_playback_ui_bookmarks_refresh(void);
void video_playback_ui_delete_bookmark(void);
void video_playback_ui_request_bookmark_delete(void);

void video_playback_ui_extra(void);
int video_playback_ui_collect(void);
double video_playback_ui_selected_position(void);
size_t video_playback_ui_selected_playlist(void);
