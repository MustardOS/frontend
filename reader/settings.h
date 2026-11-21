#pragma once

#include <common/base/options.h>

#define CHUTNEY_PATH RUN_STORAGE_PATH "chutney/"

#define READER_TEXT_MIN  12
#define READER_TEXT_MAX  48
#define READER_TEXT_STEP 2

#define READER_MARGIN_MAX  48
#define READER_MARGIN_STEP 4

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
} reader_settings;

extern const int reader_zoom_steps[];
extern const int reader_zoom_count;
extern const int reader_spacing_steps[];
extern const int reader_spacing_count;

void reader_settings_defaults(reader_settings *settings);

void reader_settings_load(reader_settings *settings);

void reader_settings_save(const reader_settings *settings);

int reader_position_load(const char *document);

void reader_position_save(const char *document, int position);
