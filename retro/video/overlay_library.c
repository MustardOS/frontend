#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <json/json.h>
#include <common/base/image_ext.h>
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

static int has_overlay_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && is_overlay_ext(dot + 1);
}

static int library_file(const char *root, const char *key, char *out, const size_t out_size) {
    int ext_count;
    const char **extensions = overlay_ext_list(&ext_count);

    for (int i = 0; i < ext_count; i++) {
        if ((size_t) snprintf(out, out_size, "%s%s.%s", root, key, extensions[i]) < out_size && file_exist(out))
            return 1;
    }

    return 0;
}

static int metadata_path(const char *image_path, char *out, const size_t out_size) {
    if (!image_path || !out || !out_size || (size_t) snprintf(out, out_size, "%s", image_path) >= out_size) return 0;
    char *extension = strrchr(out, '.');
    if (!extension || (size_t) (extension - out) + sizeof(".json") > out_size) return 0;
    snprintf(extension, out_size - (size_t) (extension - out), ".json");
    return 1;
}

static int metadata_offset(const struct json value, int *output) {
    if (json_type(value) != JSON_NUMBER) return 0;
    const int parsed = json_int(value);
    if (parsed < -4096 || parsed > 4096) return 0;
    *output = parsed;
    return 1;
}

static int metadata_viewport(const struct json object, int *x_offset, int *y_offset) {
    const struct json viewport = json_object_get(object, "viewport");
    if (json_type(viewport) == JSON_ARRAY && json_array_count(viewport) == 2)
        return metadata_offset(json_array_get(viewport, 0), x_offset)
               && metadata_offset(json_array_get(viewport, 1), y_offset);

    // Read sidecars written by builds from before viewport arrays were introduced.
    return metadata_offset(json_object_get(object, "x_offset"), x_offset)
           && metadata_offset(json_object_get(object, "y_offset"), y_offset);
}

int overlay_library_write_offsets(const char *image_path, const int x_offset, const int y_offset) {
    char path[PATH_MAX];
    if (!metadata_path(image_path, path, sizeof(path))) return 0;
    if (!x_offset && !y_offset) {
        if (unlink(path) != 0 && errno != ENOENT) return 0;
        return 1;
    }

    char temporary[PATH_MAX];
    if ((size_t) snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= sizeof(temporary)) return 0;
    const int fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (fd < 0) return 0;

    char content[96];
    const int length = snprintf(content, sizeof(content), "{\"viewport\":[%d,%d]}\n", x_offset, y_offset);
    int okay = length > 0 && (size_t) length < sizeof(content);
    if (okay) okay = write(fd, content, (size_t) length) == (ssize_t) length;
    if (okay) okay = fsync(fd) == 0;
    const int close_result = close(fd);
    if (okay) okay = close_result == 0;
    if (okay) okay = rename(temporary, path) == 0;
    if (!okay) {
        unlink(temporary);
    }
    return okay;
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
        if (!S_ISREG(status.st_mode) || !has_overlay_extension(item->d_name)) continue;

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
    if (overlay_library_dir(root, sizeof(root)) && library_file(root, entries[index].key, out, out_size)) return 1;

    return overlay_system_dir(root, sizeof(root)) && library_file(root, entries[index].key, out, out_size);
}

int overlay_library_offsets(const int index, int *x_offset, int *y_offset) {
    if (!x_offset || !y_offset) return 0;
    *x_offset = 0;
    *y_offset = 0;

    char image[PATH_MAX];
    char path[PATH_MAX];
    if (!overlay_library_path(index, image, sizeof(image)) || !metadata_path(image, path, sizeof(path))) return 0;

    struct stat info;
    if (lstat(path, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size > 256) return 0;

    char *raw = read_all_char_from(path);
    if (!raw || !json_valid(raw)) {
        free(raw);
        return 0;
    }
    const struct json root = json_parse(raw);
    const int okay = json_type(root) == JSON_OBJECT && metadata_viewport(root, x_offset, y_offset);
    free(raw);
    if (!okay) {
        *x_offset = 0;
        *y_offset = 0;
    }
    return okay;
}

int overlay_library_is_user(const int index) {
    ensure_loaded();
    if (index <= 0 || index >= entry_count) return 0;

    char root[PATH_MAX];
    char path[PATH_MAX];
    if (!overlay_library_dir(root, sizeof(root))) return 0;

    return library_file(root, entries[index].key, path, sizeof(path));
}

int overlay_library_delete(const int index) {
    ensure_loaded();
    if (!overlay_library_is_user(index)) return 0;

    char key[OVERLAY_KEY_MAX];
    snprintf(key, sizeof(key), "%s", entries[index].key);

    DIR *root = opendir(OVERLAY_IMAGE_ROOT);
    if (!root) return 0;

    int ext_count;
    const char **extensions = overlay_ext_list(&ext_count);

    int removed = 0;
    const struct dirent *item;

    while ((item = readdir(root))) {
        if (item->d_name[0] == '.') continue;

        for (int i = 0; i < ext_count; i++) {
            char path[PATH_MAX];
            if ((size_t) snprintf(path, sizeof(path), OVERLAY_IMAGE_ROOT "%s/%s.%s", item->d_name, key, extensions[i])
                >= sizeof(path))
                continue;

            if (remove(path) == 0) removed++;
        }

        char metadata[PATH_MAX];
        if ((size_t) snprintf(metadata, sizeof(metadata), OVERLAY_IMAGE_ROOT "%s/%s.json", item->d_name, key)
            < sizeof(metadata))
            remove(metadata);
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
