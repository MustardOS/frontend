#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <common/platform/device.h>
#include <common/storage/fileio.h>
#include "overlay_library.h"

#define OVERLAY_LIBRARY_MAX 128
#define OVERLAY_KEY_MAX     64
#define OVERLAY_LABEL_MAX   64

struct overlay_entry {
    char key[OVERLAY_KEY_MAX];
    char label[OVERLAY_LABEL_MAX];
};

static struct overlay_entry entries[OVERLAY_LIBRARY_MAX];
static int entry_count = -1;

int overlay_library_dir(char *out, const size_t out_size) {
    return (size_t) snprintf(out, out_size, OVERLAY_IMAGE_ROOT "%dx%d/", device.screen.width, device.screen.height)
           < out_size;
}

static int overlay_system_dir(char *out, const size_t out_size) {
    return (size_t) snprintf(out, out_size, OVERLAY_IMAGE_SYS "%dx%d/", device.screen.width, device.screen.height)
           < out_size;
}

static int has_png_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ".png") == 0;
}

static void prettify(const char *key, char *out, const size_t out_size) {
    size_t written = 0;
    int start_of_word = 1;

    for (const char *cursor = key; *cursor && written + 1 < out_size; cursor++) {
        char character = *cursor;

        if (character == '-' || character == '_') {
            character = ' ';
            start_of_word = 1;
        } else if (start_of_word) {
            character = (char) toupper((unsigned char) character);
            start_of_word = 0;
        }

        out[written++] = character;
    }

    out[written] = '\0';
}

static int entry_compare(const void *a, const void *b) {
    const struct overlay_entry *left = a;
    const struct overlay_entry *right = b;
    const int folded = strcasecmp(left->label, right->label);
    if (folded) return folded;

    const int key_folded = strcasecmp(left->key, right->key);
    return key_folded ? key_folded : strcmp(left->key, right->key);
}

static void scan_into(const char *root) {
    DIR *directory = opendir(root);
    if (!directory) return;

    const struct dirent *item;
    while ((item = readdir(directory)) && entry_count < OVERLAY_LIBRARY_MAX) {
        if (item->d_name[0] == '.' || !has_png_extension(item->d_name)) continue;

        char key[OVERLAY_KEY_MAX];
        snprintf(key, sizeof(key), "%s", item->d_name);

        char *dot = strrchr(key, '.');
        if (dot) *dot = '\0';
        if (!key[0]) continue;

        char label[OVERLAY_LABEL_MAX];
        prettify(key, label, sizeof(label));

        int seen = 0;
        for (int index = 0; index < entry_count && !seen; index++)
            seen = strcasecmp(entries[index].key, key) == 0 || strcasecmp(entries[index].label, label) == 0;
        if (seen) continue;

        snprintf(entries[entry_count].key, OVERLAY_KEY_MAX, "%s", key);
        snprintf(entries[entry_count].label, OVERLAY_LABEL_MAX, "%s", label);
        entry_count++;
    }

    closedir(directory);
}

void overlay_library_refresh(void) {
    entry_count = 0;

    char root[PATH_MAX];
    if (overlay_library_dir(root, sizeof(root))) scan_into(root);
    if (overlay_system_dir(root, sizeof(root))) scan_into(root);

    if (entry_count > 1) qsort(entries, (size_t) entry_count, sizeof(entries[0]), entry_compare);
}

static void ensure_loaded(void) {
    if (entry_count < 0) overlay_library_refresh();
}

int overlay_library_count(void) {
    ensure_loaded();
    return entry_count;
}

const char *overlay_library_label(const int index) {
    ensure_loaded();
    return index >= 0 && index < entry_count ? entries[index].label : "";
}

const char *overlay_library_key(const int index) {
    ensure_loaded();
    return index >= 0 && index < entry_count ? entries[index].key : "";
}

int overlay_library_index(const char *key) {
    if (!key || !*key) return -1;

    ensure_loaded();
    for (int index = 0; index < entry_count; index++)
        if (strcasecmp(entries[index].key, key) == 0) return index;

    return -1;
}

int overlay_library_path(const int index, char *out, const size_t out_size) {
    ensure_loaded();
    if (index < 0 || index >= entry_count) return 0;

    char root[PATH_MAX];
    if (overlay_library_dir(root, sizeof(root))
        && (size_t) snprintf(out, out_size, "%s%s.png", root, entries[index].key) < out_size && file_exist(out))
        return 1;

    if (!overlay_system_dir(root, sizeof(root))) return 0;

    return (size_t) snprintf(out, out_size, "%s%s.png", root, entries[index].key) < out_size;
}

int overlay_library_is_user(const int index) {
    ensure_loaded();
    if (index < 0 || index >= entry_count) return 0;

    char root[PATH_MAX];
    char path[PATH_MAX];
    if (!overlay_library_dir(root, sizeof(root))) return 0;
    if ((size_t) snprintf(path, sizeof(path), "%s%s.png", root, entries[index].key) >= sizeof(path)) return 0;

    return file_exist(path);
}

int overlay_library_delete(const int index) {
    ensure_loaded();
    if (!overlay_library_is_user(index)) return 0;

    char key[OVERLAY_KEY_MAX];
    snprintf(key, sizeof(key), "%s", entries[index].key);

    DIR *root = opendir(OVERLAY_IMAGE_ROOT);
    if (!root) return 0;

    int removed = 0;
    const struct dirent *item;

    while ((item = readdir(root))) {
        if (item->d_name[0] == '.') continue;

        char path[PATH_MAX];
        if ((size_t) snprintf(path, sizeof(path), OVERLAY_IMAGE_ROOT "%s/%s.png", item->d_name, key) >= sizeof(path))
            continue;

        if (remove(path) == 0) removed++;
    }

    closedir(root);

    if (removed) overlay_library_refresh();
    return removed > 0;
}

int overlay_library_file_valid(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return 0;
    if (info.st_size <= 8 || info.st_size > OVERLAY_IMAGE_MAX) return 0;

    FILE *handle = fopen(path, "rb");
    if (!handle) return 0;

    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char header[8];
    const size_t read = fread(header, 1, sizeof(header), handle);
    fclose(handle);

    return read == sizeof(header) && memcmp(header, signature, sizeof(signature)) == 0;
}
