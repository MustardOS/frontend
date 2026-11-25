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
    reader_status_clock,
    reader_status_battery,
    reader_status_both,
    reader_status_count,
} reader_status;

typedef enum {
    reader_sharpness_normal,
    reader_sharpness_high,
    reader_sharpness_max,
    reader_sharpness_count,
} reader_sharpness;

typedef enum {
    reader_hotkey_quit,
    reader_hotkey_bookmark,
    reader_hotkey_filter,
    reader_hotkey_status,
    reader_hotkey_sharpness,
    reader_hotkey_auto_turn,
    reader_hotkey_brightness_down,
    reader_hotkey_brightness_up,
    reader_hotkey_contrast_down,
    reader_hotkey_contrast_up,
    reader_hotkey_jump_back,
    reader_hotkey_jump_forward,
    reader_hotkey_chapter_back,
    reader_hotkey_chapter_forward,
    reader_hotkey_count,
} reader_hotkey;

#define READER_HOTKEY_UNBOUND (-1)

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
    int page_number;
    int readtime;
    int stay_awake;
    int menu_item;
    int transition;
    int trim;
    int sharpness;
    int auto_turn;
    int brightness;
    int contrast;
    int saturation;
    int hue_shift;
    int gamma;
    int hotkey[reader_hotkey_count];
    char filter[MAX_BUFFER_SIZE];
    char font[PATH_MAX];
} reader_settings;

extern const int reader_zoom_steps[];
extern const int reader_zoom_count;
extern const int reader_spacing_steps[];
extern const int reader_spacing_count;
extern const int reader_auto_turn_steps[];
extern const int reader_auto_turn_count;
extern const int reader_hotkey_defaults[];

#define READER_COLOUR_STEP 5

typedef enum {
    reader_scope_content,
    reader_scope_directory,
    reader_scope_count,
} reader_scope;

void reader_settings_defaults(reader_settings *settings);

void reader_settings_load(reader_settings *settings);

void reader_settings_save(const reader_settings *settings);

void reader_settings_clamp(reader_settings *settings);

void reader_scope_open(const char *document);

void reader_scope_apply(reader_settings *settings);

int reader_scope_save(const reader_settings *settings, reader_scope scope);

int reader_scope_exists(reader_scope scope);

int reader_settings_same(const reader_settings *left, const reader_settings *right);

void reader_settings_copy_scoped(reader_settings *target, const reader_settings *source);

int reader_position_load(const char *document, int *reversed);

void reader_position_save(const char *document, int position, int reversed);

int reader_bookmarks_load(const char *document, int *positions, int max);

void reader_bookmarks_save(const char *document, const int *positions, int count);

void reader_layout_cache_path(const char *document, const char *font, char *path, size_t size);

int reader_thumbnail_path(const char *document, int bookmark, int position, char *path, size_t size);
