#pragma once

#define PRESET_BROWSER_KEY_MAX   64
#define PRESET_BROWSER_LABEL_MAX 96

enum preset_browser_kind {
    preset_browser_filter = 0,
    preset_browser_shader,
    preset_browser_overlay,
    preset_browser_kind_count
};

enum preset_browser_row_type {
    preset_browser_row_download = 0,
    preset_browser_row_none,
    preset_browser_row_collection,
    preset_browser_row_directory,
    preset_browser_row_item,
    preset_browser_row_empty
};

typedef const char *(*preset_browser_text_fn)(int index);

typedef struct {
    enum preset_browser_row_type type;
    int item_index;
    char key[PRESET_BROWSER_KEY_MAX];
    char label[PRESET_BROWSER_LABEL_MAX];
    char value[PRESET_BROWSER_LABEL_MAX];
} preset_browser_row;

typedef struct {
    enum preset_browser_kind kind;
    int item_count;
    int row_count;
    int row_capacity;
    int collection;
    char directory[PRESET_BROWSER_KEY_MAX];
    preset_browser_text_fn key_fn;
    preset_browser_text_fn label_fn;
    preset_browser_row *rows;
} preset_browser;

void preset_browser_init(preset_browser *browser, enum preset_browser_kind kind);

void preset_browser_destroy(preset_browser *browser);

int preset_browser_configure(
    preset_browser *browser, int item_count, preset_browser_text_fn key_fn, preset_browser_text_fn label_fn
);

int preset_browser_open_root(preset_browser *browser);

int preset_browser_focus_key(preset_browser *browser, const char *key);

int preset_browser_enter(preset_browser *browser, int row_index);

int preset_browser_back(preset_browser *browser, char *return_key, int return_key_size);

int preset_browser_find_row(const preset_browser *browser, enum preset_browser_row_type type, const char *key);

const preset_browser_row *preset_browser_row_at(const preset_browser *browser, int row_index);

int preset_browser_is_collected(enum preset_browser_kind kind, const char *key);

int preset_browser_toggle_collection(enum preset_browser_kind kind, const char *key);

void preset_browser_remove_collection(enum preset_browser_kind kind, const char *key);
