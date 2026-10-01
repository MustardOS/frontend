#pragma once

#include <stddef.h>
#include <time.h>

typedef struct {
    char *uri;
    char *title;
    char *thumbnail;
    char *name;
    double position;
    double duration;
    time_t updated;
    int live;
} video_state_entry;

int video_state_init(void);
int video_history_load(video_state_entry **entries, size_t *count);
int video_history_find(const char *uri, video_state_entry *entry);
int video_history_update(
    const char *uri, const char *title, double position, double duration, int complete, const char *thumbnail,
    const char *target_uri, int live
);
int video_bookmark_load(video_state_entry **entries, size_t *count);
int video_bookmark_add(
    const char *uri, const char *title, const char *name, double position, double duration, const char *thumbnail
);
int video_bookmark_set_quick(
    const char *uri, const char *title, double position, double duration, const char *thumbnail
);
int video_bookmark_find_quick(const char *uri, double *position);
int video_bookmark_is_quick(const char *name);
int video_bookmark_remove(const char *uri, double position);
int video_state_thumbnail_path(const char *uri, double position, int bookmark, char *path, size_t path_size);
int video_collection_load(video_state_entry **entries, size_t *count);
int video_collection_contains(const char *uri);
int video_collection_toggle(const char *uri, const char *title, int live, int *collected);
void video_state_free(video_state_entry *entries, size_t count);
