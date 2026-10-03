#pragma once

#include <stddef.h>
#include "../content/library.h"

typedef struct {
    int hardware_decode;
    int resume;
    int deinterlace;
    int keep_history;
    int live;
    double start_position;
    const video_library_entry *playlist;
    size_t playlist_count;
    size_t playlist_index;
    int playlist_channels;
    size_t *playlist_selection;
    const char *container_uri;
    int folder_playlist;
} video_player_options;

typedef struct {
    char video_codec[64];
    char pixel_format[64];
    char audio_codec[64];
    int width;
    int height;
    double frame_rate;
    int audio_rate;
    int audio_channels;
    int queued_video;
    int queued_audio;
    double position;
    double duration;
    int live;
    int deinterlace;
    int audio_only;
    char title[256];
    char artist[256];
    char album[256];
    char album_artist[256];
    char composer[256];
    char genre[128];
    char year[64];
    char track[64];
    char disc[64];
    char comment[512];
    char copyright[256];
    char encoder[128];
    char format[128];
    int bitrate;
} video_player_info;

enum {
    video_player_failed = -1,
    video_player_stopped = 0,
    video_player_playlist_switch = 1,
    video_player_content_switch = 2,
};

int video_player_run(const char *uri, const char *title, const video_player_options *options);

void video_player_image_settings_changed(void);
void video_player_effect_settings_changed(void);

void video_player_audio_settings_changed(void);
void video_player_audio_ui_changed(void);
void video_player_modes_changed(void);
void video_player_transition_settings_changed(void);

void video_player_diagnostics(char *buffer, size_t size);

void video_player_get_information(video_player_info *information);

int video_player_live_quality_available(void);
int video_player_tracker_loop_available(void);
int video_player_speed_available(void);
int video_player_seek_available(void);
void video_player_live_quality_value(char *buffer, size_t size);
