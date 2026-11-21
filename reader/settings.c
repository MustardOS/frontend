#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <common/storage/fileio.h>
#include "settings.h"

#define SETTINGS_FILE CHUTNEY_PATH "settings.ini"
#define POSITION_DIR  CHUTNEY_PATH "position/"

const int reader_zoom_steps[] = {100, 125, 150, 200, 250, 300};
const int reader_zoom_count = (int) A_SIZE(reader_zoom_steps);
const int reader_spacing_steps[] = {0, 120, 140, 160, 180};
const int reader_spacing_count = (int) A_SIZE(reader_spacing_steps);

typedef struct {
    const char *key;
    size_t offset;
    int min;
    int max;
} setting_field;

static const setting_field fields[] = {
    {"text_size", offsetof(reader_settings, text_size), READER_TEXT_MIN, READER_TEXT_MAX},
    {"margin", offsetof(reader_settings, margin), 0, READER_MARGIN_MAX},
    {"line_spacing", offsetof(reader_settings, line_spacing), 0, (int) A_SIZE(reader_spacing_steps) - 1},
    {"fit_page", offsetof(reader_settings, fit_page), 0, 1},
    {"zoom", offsetof(reader_settings, zoom), 0, (int) A_SIZE(reader_zoom_steps) - 1},
    {"theme", offsetof(reader_settings, theme), 0, reader_theme_count - 1},
    {"rotation", offsetof(reader_settings, rotation), 0, reader_rotation_count - 1},
    {"page_status", offsetof(reader_settings, page_status), 0, 1},
    {"stay_awake", offsetof(reader_settings, stay_awake), 0, 1},
    {"menu_item", offsetof(reader_settings, menu_item), 0, 63},
    {"transition", offsetof(reader_settings, transition), 0, reader_transition_count - 1},
};

void reader_settings_defaults(reader_settings *settings) {
    *settings = (reader_settings) {
        .text_size = 22,
        .margin = 16,
        .line_spacing = 0,
        .fit_page = 0,
        .zoom = 0,
        .theme = reader_theme_light,
        .rotation = reader_rotation_none,
        .page_status = 1,
        .stay_awake = 1,
        .menu_item = 0,
        .transition = reader_transition_crossfade,
    };
}

void reader_settings_load(reader_settings *settings) {
    reader_settings_defaults(settings);

    FILE *file = fopen(SETTINGS_FILE, "r");
    if (!file) return;

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char key[64];
        int value = 0;
        if (sscanf(line, "%63[^=]=%d", key, &value) != 2) continue;

        for (size_t i = 0; i < A_SIZE(fields); i++) {
            if (strcmp(key, fields[i].key) != 0) continue;
            if (value < fields[i].min || value > fields[i].max) break;
            *(int *) ((char *) settings + fields[i].offset) = value;
            break;
        }
    }

    fclose(file);
}

void reader_settings_save(const reader_settings *settings) {
    create_directories(SETTINGS_FILE, 1);

    char temp[PATH_MAX];
    snprintf(temp, sizeof(temp), "%s.tmp", SETTINGS_FILE);

    FILE *file = fopen(temp, "w");
    if (!file) return;

    for (size_t i = 0; i < A_SIZE(fields); i++)
        fprintf(file, "%s=%d\n", fields[i].key, *(const int *) ((const char *) settings + fields[i].offset));

    if (fclose(file) == 0) file_rename(temp, SETTINGS_FILE);
}

static void position_path(const char *document, char *path, const size_t size) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) document; *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    snprintf(path, size, "%s%016llx.pos", POSITION_DIR, (unsigned long long) hash);
}

int reader_position_load(const char *document) {
    char path[PATH_MAX];
    position_path(document, path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int value = 0;
    const int got = fscanf(file, "%d", &value);
    fclose(file);

    return got == 1 && value >= 0 ? value : 0;
}

void reader_position_save(const char *document, const int position) {
    char path[PATH_MAX];
    position_path(document, path, sizeof(path));
    create_directories(path, 1);

    FILE *file = fopen(path, "w");
    if (!file) return;

    fprintf(file, "%d\n", position);
    fclose(file);
}
