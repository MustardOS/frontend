#pragma once

#include <limits.h>
#include <common/base/options.h>

#define CHUTNEY_PATH RUN_STORAGE_PATH "chutney/"

#define READER_TEXT_MIN  12
#define READER_TEXT_MAX  48
#define READER_TEXT_STEP 2

#define READER_MARGIN_MAX  48
#define READER_MARGIN_STEP 4

#define READER_BOOKMARK_MAX 64

typedef enum {
    reader_theme_light,
    reader_theme_sepia,
    reader_theme_dark,
    reader_theme_night,
    reader_theme_count,
} reader_theme;

typedef enum {
    reader_rotation_none,
    reader_rotation_right,
    reader_rotation_left,
    reader_rotation_upside,
    reader_rotation_count,
} reader_rotation;

typedef enum {
    reader_status_off,
    reader_status_page,
    reader_status_full,
    reader_status_count,
} reader_status;

typedef enum {
    reader_contrast_normal,
    reader_contrast_high,
    reader_contrast_max,
    reader_contrast_count,
} reader_contrast;

typedef enum {
    reader_transition_instant,
    reader_transition_crossfade,
    reader_transition_slide,
    reader_transition_page_turn,
    reader_transition_count,
} reader_transition;

typedef struct {
    int text_size;
    int margin;
    int line_spacing;
    int fit_page;
    int zoom;
    int theme;
    int rotation;
    int page_status;
    int stay_awake;
    int menu_item;
    int transition;
    int trim;
    int contrast;
    int auto_turn;
    char font[PATH_MAX];
} reader_settings;

extern const int reader_zoom_steps[];
extern const int reader_zoom_count;
extern const int reader_spacing_steps[];
extern const int reader_spacing_count;
extern const int reader_auto_turn_steps[];
extern const int reader_auto_turn_count;

void reader_settings_defaults(reader_settings *settings);

void reader_settings_load(reader_settings *settings);

void reader_settings_save(const reader_settings *settings);

int reader_position_load(const char *document, int *reversed);

void reader_position_save(const char *document, int position, int reversed);

int reader_bookmarks_load(const char *document, int *positions, int max);

void reader_bookmarks_save(const char *document, const int *positions, int count);

void reader_layout_cache_path(const char *document, const char *font, char *path, size_t size);
