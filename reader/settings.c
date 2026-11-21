#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <common/config/config.h>
#include <common/storage/fileio.h>
#include "settings.h"

#define SETTINGS_FILE CHUTNEY_PATH "settings.ini"
#define POSITION_DIR  CHUTNEY_PATH "position/"
#define BOOKMARK_DIR  CHUTNEY_PATH "bookmark/"
#define LAYOUT_DIR    CHUTNEY_PATH "layout/"

const int reader_zoom_steps[] = {100, 125, 150, 200, 250, 300};
const int reader_zoom_count = (int) A_SIZE(reader_zoom_steps);
const int reader_spacing_steps[] = {0, 120, 140, 160, 180};
const int reader_spacing_count = (int) A_SIZE(reader_spacing_steps);
const int reader_auto_turn_steps[] = {0, 10, 15, 20, 30, 45, 60, 90, 120};
const int reader_auto_turn_count = (int) A_SIZE(reader_auto_turn_steps);

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
    {"page_status", offsetof(reader_settings, page_status), 0, reader_status_count - 1},
    {"stay_awake", offsetof(reader_settings, stay_awake), 0, 1},
    {"menu_item", offsetof(reader_settings, menu_item), 0, 63},
    {"transition", offsetof(reader_settings, transition), 0, reader_transition_count - 1},
    {"trim", offsetof(reader_settings, trim), 0, 1},
    {"contrast", offsetof(reader_settings, contrast), 0, reader_contrast_count - 1},
    {"auto_turn", offsetof(reader_settings, auto_turn), 0, (int) A_SIZE(reader_auto_turn_steps) - 1},
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
        .page_status = reader_status_page,
        .stay_awake = 1,
        .menu_item = 0,
        .transition = reader_transition_crossfade,
        .trim = 0,
        .contrast = reader_contrast_normal,
        .auto_turn = 0,
    };
}

void reader_settings_load(reader_settings *settings) {
    reader_settings_defaults(settings);

    FILE *file = fopen(SETTINGS_FILE, "r");
    if (!file) return;

    char line[PATH_MAX + 16];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "font=", 5) == 0) {
            snprintf(settings->font, sizeof(settings->font), "%s", line + 5);
            settings->font[strcspn(settings->font, "\r\n")] = '\0';
            continue;
        }

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
    fprintf(file, "font=%s\n", settings->font);

    if (fclose(file) == 0) file_rename(temp, SETTINGS_FILE);
}

#define HASH_SEED UINT64_C(1469598103934665603)

static uint64_t hash_bytes(uint64_t hash, const void *data, const size_t size) {
    const unsigned char *cursor = data;
    for (size_t i = 0; i < size; i++) {
        hash ^= cursor[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_text(const uint64_t hash, const char *text) {
    return hash_bytes(hash, text, strlen(text));
}

static void
document_path(const char *directory, const char *document, const char *suffix, char *path, const size_t size) {
    snprintf(path, size, "%s%016llx.%s", directory, (unsigned long long) hash_text(HASH_SEED, document), suffix);
}

static void position_path(const char *document, char *path, const size_t size) {
    document_path(POSITION_DIR, document, "pos", path, size);
}

/* Written beside the target then renamed over it so a power cut never leaves a half written file */
static FILE *open_atomic(const char *path, char *temp, const size_t size) {
    create_directories(path, 1);
    snprintf(temp, size, "%s.tmp", path);
    return fopen(temp, "w");
}

static void close_atomic(FILE *file, const char *temp, const char *path) {
    if (fclose(file) == 0)
        file_rename(temp, path);
    else
        remove(temp);
}

int reader_position_load(const char *document, int *reversed) {
    *reversed = 0;

    char path[PATH_MAX];
    position_path(document, path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int value = 0;
    int order = 0;
    const int got = fscanf(file, "%d %d", &value, &order);
    fclose(file);

    if (got == 2) *reversed = order == 1;
    return got >= 1 && value >= 0 ? value : 0;
}

void reader_position_save(const char *document, const int position, const int reversed) {
    char path[PATH_MAX];
    char temp[PATH_MAX + 8];
    position_path(document, path, sizeof(path));

    FILE *file = open_atomic(path, temp, sizeof(temp));
    if (!file) return;

    fprintf(file, "%d %d\n", position, reversed ? 1 : 0);
    close_atomic(file, temp, path);
}

int reader_bookmarks_load(const char *document, int *positions, const int max) {
    char path[PATH_MAX];
    document_path(BOOKMARK_DIR, document, "bmk", path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int count = 0;
    int value = 0;
    while (count < max && fscanf(file, "%d", &value) == 1)
        if (value >= 0) positions[count++] = value;

    fclose(file);
    return count;
}

void reader_bookmarks_save(const char *document, const int *positions, const int count) {
    char path[PATH_MAX];
    document_path(BOOKMARK_DIR, document, "bmk", path, sizeof(path));

    if (count <= 0) {
        remove(path);
        return;
    }

    char temp[PATH_MAX + 8];
    FILE *file = open_atomic(path, temp, sizeof(temp));
    if (!file) return;

    for (int i = 0; i < count; i++)
        fprintf(file, "%d\n", positions[i]);
    close_atomic(file, temp, path);
}

void reader_layout_cache_path(const char *document, const char *font, char *path, const size_t size) {
    /* Anything that changes where the pages break has to be part of the name */
    uint64_t hash = hash_text(HASH_SEED, document);
    hash = hash_text(hash, "\n");
    hash = hash_text(hash, font ? font : "");
    hash = hash_text(hash, "\n");
    hash = hash_text(hash, config.settings.general.language);

    struct stat info;
    if (stat(document, &info) == 0) {
        const int64_t stamp[2] = {(int64_t) info.st_size, (int64_t) info.st_mtime};
        hash = hash_bytes(hash, stamp, sizeof(stamp));
    }

    snprintf(path, size, "%s%016llx.acc", LAYOUT_DIR, (unsigned long long) hash);
    create_directories(path, 1);
}
