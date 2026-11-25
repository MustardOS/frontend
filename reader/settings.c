#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <common/config/config.h>
#include <common/platform/input.h>
#include <common/storage/fileio.h>
#include "settings.h"

#define SETTINGS_FILE CHUTNEY_PATH "settings.ini"
#define POSITION_DIR  CHUTNEY_PATH "position/"
#define BOOKMARK_DIR  CHUTNEY_PATH "bookmark/"
#define LAYOUT_DIR    CHUTNEY_PATH "layout/"
#define SCOPE_DIR     CHUTNEY_PATH "settings/"
#define STATE_DIR     CHUTNEY_PATH "state/"

const int reader_zoom_steps[] = {100, 125, 150, 200, 250, 300};
const int reader_zoom_count = (int) A_SIZE(reader_zoom_steps);
const int reader_spacing_steps[] = {0, 120, 140, 160, 180};
const int reader_spacing_count = (int) A_SIZE(reader_spacing_steps);
const int reader_auto_turn_steps[] = {0, 10, 15, 20, 30, 45, 60, 90, 120};
const int reader_auto_turn_count = (int) A_SIZE(reader_auto_turn_steps);
const int reader_hotkey_defaults[reader_hotkey_count] = {
    mux_input_start,     mux_input_a,          mux_input_x,       mux_input_y,  mux_input_select,
    mux_input_b,         mux_input_dpad_down,  mux_input_dpad_up, mux_input_l2, mux_input_r2,
    mux_input_dpad_left, mux_input_dpad_right, mux_input_l1,      mux_input_r1,
};

typedef struct {
    const char *key;
    size_t offset;
    int min;
    int max;
    int scoped;
} setting_field;

static const setting_field fields[] = {
    {"text_size", offsetof(reader_settings, text_size), READER_TEXT_MIN, READER_TEXT_MAX, 1},
    {"margin", offsetof(reader_settings, margin), 0, READER_MARGIN_MAX, 1},
    {"line_spacing", offsetof(reader_settings, line_spacing), 0, (int) A_SIZE(reader_spacing_steps) - 1, 1},
    {"fit_page", offsetof(reader_settings, fit_page), 0, 1, 1},
    {"zoom", offsetof(reader_settings, zoom), 0, (int) A_SIZE(reader_zoom_steps) - 1, 1},
    {"theme", offsetof(reader_settings, theme), 0, reader_theme_count - 1, 1},
    {"rotation", offsetof(reader_settings, rotation), 0, reader_rotation_count - 1, 1},
    {"header_visibility", offsetof(reader_settings, page_status), 0, reader_status_count - 1, 1},
    {"stay_awake", offsetof(reader_settings, stay_awake), 0, 1, 1},
    {"page_number", offsetof(reader_settings, page_number), 0, 1, 1},
    {"readtime", offsetof(reader_settings, readtime), 0, 1, 1},
    {"menu_item", offsetof(reader_settings, menu_item), 0, 63, 0},
    {"transition", offsetof(reader_settings, transition), 0, reader_transition_count - 1, 1},
    {"trim", offsetof(reader_settings, trim), 0, 1, 1},
    {"sharpness", offsetof(reader_settings, sharpness), 0, reader_sharpness_count - 1, 1},
    {"auto_turn", offsetof(reader_settings, auto_turn), 0, (int) A_SIZE(reader_auto_turn_steps) - 1, 1},
    {"colour_brightness", offsetof(reader_settings, brightness), -100, 100, 1},
    {"colour_contrast", offsetof(reader_settings, contrast), 0, 200, 1},
    {"colour_saturation", offsetof(reader_settings, saturation), 0, 200, 1},
    {"colour_hue_shift", offsetof(reader_settings, hue_shift), -180, 180, 1},
    {"colour_gamma", offsetof(reader_settings, gamma), 50, 200, 1},
#define HOTKEY_FIELD(NAME, INDEX)                                                                                      \
    {"hotkey_" NAME, offsetof(reader_settings, hotkey) + (INDEX) * sizeof(int), READER_HOTKEY_UNBOUND,                 \
     mux_input_count - 1, 0}
    HOTKEY_FIELD("quit", reader_hotkey_quit),
    HOTKEY_FIELD("bookmark", reader_hotkey_bookmark),
    HOTKEY_FIELD("filter", reader_hotkey_filter),
    HOTKEY_FIELD("status", reader_hotkey_status),
    HOTKEY_FIELD("sharpness", reader_hotkey_sharpness),
    HOTKEY_FIELD("auto_turn", reader_hotkey_auto_turn),
    HOTKEY_FIELD("brightness_down", reader_hotkey_brightness_down),
    HOTKEY_FIELD("brightness_up", reader_hotkey_brightness_up),
    HOTKEY_FIELD("contrast_down", reader_hotkey_contrast_down),
    HOTKEY_FIELD("contrast_up", reader_hotkey_contrast_up),
    HOTKEY_FIELD("jump_back", reader_hotkey_jump_back),
    HOTKEY_FIELD("jump_forward", reader_hotkey_jump_forward),
    HOTKEY_FIELD("chapter_back", reader_hotkey_chapter_back),
    HOTKEY_FIELD("chapter_forward", reader_hotkey_chapter_forward),
#undef HOTKEY_FIELD
};

_Static_assert(A_SIZE(reader_hotkey_defaults) == reader_hotkey_count, "Chutney hotkey defaults must match");

static char scope_files[reader_scope_count][PATH_MAX];

void reader_settings_defaults(reader_settings *settings) {
    *settings = (reader_settings) {
        .text_size = 22,
        .margin = 16,
        .line_spacing = 0,
        .fit_page = 0,
        .zoom = 0,
        .theme = reader_theme_light,
        .rotation = reader_rotation_none,
        .page_status = reader_status_off,
        .page_number = 1,
        .readtime = 0,
        .stay_awake = 1,
        .menu_item = 0,
        .transition = reader_transition_crossfade,
        .trim = 0,
        .sharpness = reader_sharpness_normal,
        .auto_turn = 0,
        .brightness = 0,
        .contrast = 100,
        .saturation = 100,
        .hue_shift = 0,
        .gamma = 100,
    };
    snprintf(settings->filter, sizeof(settings->filter), "none");
    for (int i = 0; i < reader_hotkey_count; i++)
        settings->hotkey[i] = reader_hotkey_defaults[i];
}

static void copy_text(char *target, const size_t size, const char *value) {
    snprintf(target, size, "%s", value);
    target[strcspn(target, "\r\n")] = '\0';
}

static int load_file(reader_settings *settings, const char *path, const int scoped_only) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;

    char line[PATH_MAX + 16];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "font=", 5) == 0) {
            copy_text(settings->font, sizeof(settings->font), line + 5);
            continue;
        }
        if (strncmp(line, "colour_filter=", 14) == 0) {
            copy_text(settings->filter, sizeof(settings->filter), line + 14);
            if (!settings->filter[0]) snprintf(settings->filter, sizeof(settings->filter), "none");
            continue;
        }

        char key[64];
        int value = 0;
        if (sscanf(line, "%63[^=]=%d", key, &value) != 2) continue;

        if (strcmp(key, "page_status") == 0) {
            if (value >= 0 && value <= 2) {
                settings->page_status = value == 2 ? reader_status_both : reader_status_off;
                settings->page_number = value > 0;
            }
            continue;
        }

        if (strcmp(key, "header_visibility") == 0 && (value == 4 || value == 5)) {
            settings->page_status = value == 5 ? reader_status_both : reader_status_off;
            settings->page_number = 1;
            continue;
        }

        if (strcmp(key, "contrast") == 0) {
            if (value >= 0 && value < reader_sharpness_count) settings->sharpness = value;
            continue;
        }

        for (size_t i = 0; i < A_SIZE(fields); i++) {
            if (strcmp(key, fields[i].key) != 0) continue;
            if (value < fields[i].min || value > fields[i].max || (scoped_only && !fields[i].scoped)) break;
            *(int *) ((char *) settings + fields[i].offset) = value;
            break;
        }
    }

    fclose(file);
    return 1;
}

static int save_file(const reader_settings *settings, const char *path, const int scoped_only) {
    create_directories(path, 1);

    char temp[PATH_MAX];
    snprintf(temp, sizeof(temp), "%s.tmp", path);

    FILE *file = fopen(temp, "w");
    if (!file) return 0;

    for (size_t i = 0; i < A_SIZE(fields); i++)
        if (!scoped_only || fields[i].scoped)
            fprintf(file, "%s=%d\n", fields[i].key, *(const int *) ((const char *) settings + fields[i].offset));
    fprintf(file, "colour_filter=%s\n", settings->filter);
    fprintf(file, "font=%s\n", settings->font);

    return fclose(file) == 0 && file_rename(temp, path) == 0;
}

void reader_settings_load(reader_settings *settings) {
    reader_settings_defaults(settings);
    load_file(settings, SETTINGS_FILE, 0);
}

void reader_settings_clamp(reader_settings *settings) {
    for (size_t i = 0; i < A_SIZE(fields); i++) {
        int *value = (int *) ((char *) settings + fields[i].offset);
        if (*value < fields[i].min) *value = fields[i].min;
        if (*value > fields[i].max) *value = fields[i].max;
    }
}

void reader_settings_save(const reader_settings *settings) {
    save_file(settings, SETTINGS_FILE, 0);
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

void reader_scope_open(const char *document) {
    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", document ? document : "");
    char *slash = strrchr(directory, '/');
    if (slash && slash > directory) *slash = '\0';

    document_path(SCOPE_DIR "content/", document ? document : "", "ini", scope_files[reader_scope_content], PATH_MAX);
    document_path(SCOPE_DIR "directory/", directory, "ini", scope_files[reader_scope_directory], PATH_MAX);
}

void reader_scope_apply(reader_settings *settings) {
    load_file(settings, scope_files[reader_scope_directory], 1);
    load_file(settings, scope_files[reader_scope_content], 1);
}

int reader_scope_save(const reader_settings *settings, const reader_scope scope) {
    if (scope < 0 || scope >= reader_scope_count || !scope_files[scope][0]) return 0;
    return save_file(settings, scope_files[scope], 1);
}

int reader_scope_exists(const reader_scope scope) {
    return scope >= 0 && scope < reader_scope_count && scope_files[scope][0] && file_exist(scope_files[scope]);
}

int reader_settings_same(const reader_settings *left, const reader_settings *right) {
    for (size_t i = 0; i < A_SIZE(fields); i++) {
        if (!fields[i].scoped) continue;
        if (*(const int *) ((const char *) left + fields[i].offset)
            != *(const int *) ((const char *) right + fields[i].offset))
            return 0;
    }
    return strcmp(left->filter, right->filter) == 0 && strcmp(left->font, right->font) == 0;
}

void reader_settings_copy_scoped(reader_settings *target, const reader_settings *source) {
    for (size_t i = 0; i < A_SIZE(fields); i++)
        if (fields[i].scoped)
            *(int *) ((char *) target + fields[i].offset) = *(const int *) ((const char *) source + fields[i].offset);
    snprintf(target->filter, sizeof(target->filter), "%s", source->filter);
    snprintf(target->font, sizeof(target->font), "%s", source->font);
}

int reader_thumbnail_path(const char *document, const int bookmark, const int position, char *path, const size_t size) {
    if (!document || !document[0]) return 0;

    char directory[PATH_MAX];
    if (snprintf(directory, sizeof(directory), STATE_DIR "%016llx", (unsigned long long) hash_text(HASH_SEED, document))
        >= (int) sizeof(directory))
        return 0;
    create_directories(directory, 0);

    const int written = bookmark ? snprintf(path, size, "%s/bookmark_%012d.png", directory, position)
                                 : snprintf(path, size, "%s/exit.png", directory);
    return written > 0 && (size_t) written < size;
}
