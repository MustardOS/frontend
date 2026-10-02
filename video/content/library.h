#pragma once

#include <stddef.h>

typedef struct {
    char *uri;
    char *title;
    char *logo;
    int live;
} video_library_entry;

typedef enum {
    video_playlist_unavailable = -1,
    video_playlist_m3u = 0,
    video_playlist_hls = 1,
} video_playlist_type;

int video_library_scan(const char *root, int audio, video_library_entry **entries, size_t *count);
int video_playlist_load(const char *path, int live, video_library_entry **entries, size_t *count);
video_playlist_type video_playlist_probe(const char *path);
size_t video_playlist_step(
    const video_library_entry *entries, size_t count, size_t current, int direction, size_t steps, int wrap
);
size_t video_playlist_skip(
    const video_library_entry *entries, size_t count, size_t current, int direction, size_t page_size,
    int letter_skip
);
int video_path_extension_is(const char *path, const char *extension);
int video_path_is_audio(const char *path);
int video_live_scan(const char *root, video_library_entry **entries, size_t *count);
void video_library_free(video_library_entry *entries, size_t count);
const char *video_title_from_uri(const char *uri, char *buffer, size_t size);
