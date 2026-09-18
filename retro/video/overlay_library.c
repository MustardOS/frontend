#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <common/display/language.h>
#include <common/platform/device.h>
#include <common/storage/fileio.h>
#include "overlay_library.h"

#define OVERLAY_KEY_MAX   64
#define OVERLAY_LABEL_MAX 64

struct overlay_entry {
    char key[OVERLAY_KEY_MAX];
    char label[OVERLAY_LABEL_MAX];
};

static struct overlay_entry *entries;
static int entry_count = -1;
static int entry_capacity;

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
    const char *base = strrchr(key, '/');
    if (base) key = base + 1;

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

static int append_entry(const char *key) {
    for (int index = 1; index < entry_count; index++)
        if (strcasecmp(entries[index].key, key) == 0) return 1;

    if (entry_count == entry_capacity) {
        const int capacity = entry_capacity ? entry_capacity * 2 : 32;
        void *next = realloc(entries, (size_t) capacity * sizeof(*entries));
        if (!next) return 0;
        entries = next;
        entry_capacity = capacity;
    }

    snprintf(entries[entry_count].key, sizeof(entries[entry_count].key), "%s", key);
    prettify(key, entries[entry_count].label, sizeof(entries[entry_count].label));
    entry_count++;
    return 1;
}

static int scan_into(const char *root, const char *prefix, const int depth) {
    char path[PATH_MAX];
    if ((size_t) snprintf(path, sizeof(path), "%s%s", root, prefix ? prefix : "") >= sizeof(path)) return 1;

    DIR *directory = opendir(path);
    if (!directory) return 1;

    const struct dirent *item;
    int okay = 1;
    while (okay && (item = readdir(directory))) {
        if (item->d_name[0] == '.') continue;

        char key[OVERLAY_KEY_MAX];
        if ((size_t) snprintf(key, sizeof(key), "%s%s", prefix ? prefix : "", item->d_name) >= sizeof(key)) continue;

        char item_path[PATH_MAX];
        if ((size_t) snprintf(item_path, sizeof(item_path), "%s%s", root, key) >= sizeof(item_path)) continue;

        struct stat status;
        if (lstat(item_path, &status) != 0) continue;
        if (S_ISDIR(status.st_mode)) {
            if (depth >= 1) continue;
            char nested[OVERLAY_KEY_MAX];
            if ((size_t) snprintf(nested, sizeof(nested), "%s/", key) >= sizeof(nested)) continue;
            okay = scan_into(root, nested, depth + 1);
            continue;
        }
        if (!S_ISREG(status.st_mode) || !has_png_extension(item->d_name)) continue;

        char *dot = strrchr(key, '.');
        if (dot) *dot = '\0';
        if (!key[0] || strcasecmp(key, "none") == 0) continue;
        okay = append_entry(key);
    }

    closedir(directory);
    return okay;
}

void overlay_library_refresh(void) {
    entry_count = 0;
    if (!append_entry("none")) return;
    snprintf(entries[0].label, sizeof(entries[0].label), "%s", lang.generic.none);

    char root[PATH_MAX];
    if (overlay_library_dir(root, sizeof(root))) scan_into(root, NULL, 0);
    if (overlay_system_dir(root, sizeof(root))) scan_into(root, NULL, 0);

    if (entry_count > 2) qsort(entries + 1, (size_t) entry_count - 1, sizeof(entries[0]), entry_compare);
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
    if (index <= 0 || index >= entry_count) return 0;

    char root[PATH_MAX];
    if (overlay_library_dir(root, sizeof(root))
        && (size_t) snprintf(out, out_size, "%s%s.png", root, entries[index].key) < out_size && file_exist(out))
        return 1;

    if (!overlay_system_dir(root, sizeof(root))) return 0;

    return (size_t) snprintf(out, out_size, "%s%s.png", root, entries[index].key) < out_size;
}

int overlay_library_is_user(const int index) {
    ensure_loaded();
    if (index <= 0 || index >= entry_count) return 0;

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
