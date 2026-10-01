#include "assets.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <module/muxshare.h>
#include <common/base/options.h>
#include <common/storage/fileio.h>
#include "../core/paths.h"

#define ASSET_KEY_MAX   128
#define ASSET_LABEL_MAX 96

typedef struct {
    char key[ASSET_KEY_MAX];
    char label[ASSET_LABEL_MAX];
    char path[PATH_MAX];
} asset_entry;

typedef struct {
    asset_entry *entries;
    int count;
    int capacity;
} asset_store;

typedef struct {
    wasabi_asset_row_type type;
    int item;
    char key[ASSET_KEY_MAX];
    char label[ASSET_LABEL_MAX];
} browser_row;

typedef struct {
    wasabi_asset_kind kind;
    browser_row *rows;
    int count;
    int capacity;
    int collection;
    char directory[ASSET_KEY_MAX];
} asset_browser;

static asset_store stores[wasabi_asset_kind_count];
static asset_browser browser;

static const char *extension(const wasabi_asset_kind kind) {
    if (kind == wasabi_asset_filter) return ".ini";
    if (kind == wasabi_asset_shader) return ".frag";
    return ".png";
}

static int compare_entries(const void *left, const void *right) {
    const asset_entry *a = left;
    const asset_entry *b = right;
    const int folded = strcasecmp(a->label, b->label);
    return folded ? folded : strcmp(a->label, b->label);
}

static void prettify(const char *key, char *output, const size_t size) {
    const char *base = strrchr(key, '/');
    base = base ? base + 1 : key;
    size_t used = 0;
    int word = 1;
    while (*base && used + 1 < size) {
        unsigned char character = (unsigned char) *base++;
        if (character == '-' || character == '_') {
            output[used++] = ' ';
            word = 1;
        } else {
            output[used++] = word ? (char) toupper(character) : (char) character;
            word = 0;
        }
    }
    output[used] = '\0';
}

static void preset_label(const char *path, const char *key, char *output, const size_t size) {
    char metadata_path[PATH_MAX];
    snprintf(metadata_path, sizeof(metadata_path), "%s.name", path);
    FILE *metadata = fopen(metadata_path, "r");
    if (metadata) {
        if (fgets(output, (int) size, metadata)) {
            output[strcspn(output, "\r\n")] = '\0';
            fclose(metadata);
            if (output[0]) return;
        } else {
            fclose(metadata);
        }
    }

    const char *path_extension = strrchr(path, '.');
    FILE *file = path_extension && strcasecmp(path_extension, ".png") == 0 ? NULL : fopen(path, "r");
    if (file) {
        char line[256];
        int profile = 0;
        for (int index = 0; index < 32 && fgets(line, sizeof(line), file); index++) {
            char *name = NULL;
            char *cursor = line;
            while (*cursor && isspace((unsigned char) *cursor)) cursor++;
            if (*cursor == '[') {
                profile = strncasecmp(cursor, "[profile]", 9) == 0;
                continue;
            }
            if (strncasecmp(cursor, "//", 2) == 0) {
                cursor += 2;
                while (*cursor && isspace((unsigned char) *cursor)) cursor++;
            } else if (*cursor == '#') {
                cursor++;
                while (*cursor && isspace((unsigned char) *cursor)) cursor++;
            }
            if (strncasecmp(cursor, "Name:", 5) == 0)
                name = cursor + 5;
            else if (profile && strncasecmp(cursor, "name", 4) == 0) {
                char *equals = strchr(cursor + 4, '=');
                if (equals) name = equals + 1;
            }
            if (!name) continue;
            while (*name && isspace((unsigned char) *name)) name++;
            snprintf(output, size, "%s", name);
            output[strcspn(output, "\r\n")] = '\0';
            if (output[0]) {
                fclose(file);
                return;
            }
            break;
        }
        fclose(file);
    }
    prettify(key, output, size);
}

static int append(asset_store *store, const char *key, const char *path) {
    for (int index = 1; index < store->count; index++)
        if (strcasecmp(store->entries[index].key, key) == 0) return 1;
    if (store->count >= store->capacity) {
        const int capacity = store->capacity ? store->capacity * 2 : 32;
        void *next = realloc(store->entries, (size_t) capacity * sizeof(*store->entries));
        if (!next) return 0;
        store->entries = next;
        store->capacity = capacity;
    }
    asset_entry *entry = &store->entries[store->count++];
    snprintf(entry->key, sizeof(entry->key), "%s", key);
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    preset_label(path, key, entry->label, sizeof(entry->label));
    return 1;
}

static void scan(
    asset_store *store, const char *root, const char *prefix, const char *suffix, const int depth,
    const int bundled
) {
    char directory_path[PATH_MAX];
    snprintf(directory_path, sizeof(directory_path), "%s%s", root, prefix ? prefix : "");
    DIR *directory = opendir(directory_path);
    if (!directory) return;
    const struct dirent *item;
    while ((item = readdir(directory))) {
        if (item->d_name[0] == '.') continue;
        char key[ASSET_KEY_MAX];
        snprintf(key, sizeof(key), "%s%s", prefix ? prefix : "", item->d_name);
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%s", root, key);
        struct stat status;
        if (lstat(path, &status) != 0) continue;
        if (S_ISDIR(status.st_mode) && depth == 0) {
            char nested[ASSET_KEY_MAX];
            snprintf(nested, sizeof(nested), "%s/", key);
            scan(store, root, nested, suffix, 1, bundled);
            continue;
        }
        if (!S_ISREG(status.st_mode)) continue;
        const size_t name_length = strlen(key);
        const size_t suffix_length = strlen(suffix);
        if (name_length <= suffix_length || strcasecmp(key + name_length - suffix_length, suffix) != 0) continue;
        key[name_length - suffix_length] = '\0';
        if (bundled && !strchr(key, '/')) {
            char bundled_key[ASSET_KEY_MAX];
            snprintf(bundled_key, sizeof(bundled_key), "MustardOS/%s", key);
            append(store, bundled_key, path);
        } else {
            append(store, key, path);
        }
    }
    closedir(directory);
}

void wasabi_assets_refresh(const wasabi_asset_kind kind) {
    if (kind < 0 || kind >= wasabi_asset_kind_count) return;
    asset_store *store = &stores[kind];
    store->count = 0;
    append(store, "none", "");
    snprintf(store->entries[0].label, sizeof(store->entries[0].label), "%s", lang.generic.none);

    char system_root[PATH_MAX];
    char user_root[PATH_MAX];
    if (kind == wasabi_asset_filter) {
        snprintf(system_root, sizeof(system_root), OPT_SHARE_PATH "filter/");
        snprintf(user_root, sizeof(user_root), WASABI_FILTER_PATH);
    } else if (kind == wasabi_asset_shader) {
        snprintf(system_root, sizeof(system_root), OPT_SHARE_PATH "shader/");
        snprintf(user_root, sizeof(user_root), WASABI_SHADER_PATH);
    } else {
        snprintf(system_root, sizeof(system_root), OPT_SHARE_PATH "overlay/image/%dx%d/", device.screen.width,
                 device.screen.height);
        snprintf(user_root, sizeof(user_root), WASABI_OVERLAY_PATH "%dx%d/", device.screen.width,
                 device.screen.height);
    }
    scan(store, user_root, NULL, extension(kind), 0, 0);
    scan(store, system_root, NULL, extension(kind), 0, 1);
    if (store->count > 2) qsort(store->entries + 1, (size_t) store->count - 1, sizeof(*store->entries), compare_entries);
}

static asset_entry *entry_at(const wasabi_asset_kind kind, const int index) {
    if (kind < 0 || kind >= wasabi_asset_kind_count) return NULL;
    asset_store *store = &stores[kind];
    if (!store->count) wasabi_assets_refresh(kind);
    return index >= 0 && index < store->count ? &store->entries[index] : NULL;
}

int wasabi_assets_count(const wasabi_asset_kind kind) {
    if (kind < 0 || kind >= wasabi_asset_kind_count) return 0;
    if (!stores[kind].count) wasabi_assets_refresh(kind);
    return stores[kind].count;
}

const char *wasabi_asset_key(const wasabi_asset_kind kind, const int index) {
    asset_entry *entry = entry_at(kind, index);
    return entry ? entry->key : "none";
}

const char *wasabi_asset_label(const wasabi_asset_kind kind, const int index) {
    asset_entry *entry = entry_at(kind, index);
    return entry ? entry->label : lang.generic.none;
}

const char *wasabi_asset_path(const wasabi_asset_kind kind, const int index) {
    asset_entry *entry = entry_at(kind, index);
    return entry ? entry->path : "";
}

const char *wasabi_asset_display_value(const wasabi_asset_kind kind) {
    wasabi_assets_refresh(kind);
    const int selected = wasabi_asset_selected(kind);
    return selected > 0 ? wasabi_asset_label(kind, selected) : lang.generic.none;
}

static char *selected_key(const wasabi_asset_kind kind) {
    if (kind == wasabi_asset_filter) return config.video.colour_filter;
    if (kind == wasabi_asset_shader) return config.video.shader;
    return config.video.overlay_image;
}

int wasabi_asset_selected(const wasabi_asset_kind kind) {
    const int count = wasabi_assets_count(kind);
    const char *selected = selected_key(kind);
    for (int index = 0; index < count; index++)
        if (strcasecmp(wasabi_asset_key(kind, index), selected) == 0) return index;
    return 0;
}

int wasabi_asset_find(const wasabi_asset_kind kind, const char *key) {
    if (!key || !key[0]) return 0;
    const int count = wasabi_assets_count(kind);
    for (int index = 0; index < count; index++)
        if (strcasecmp(wasabi_asset_key(kind, index), key) == 0) return index;
    return 0;
}

int wasabi_asset_preview(const wasabi_asset_kind kind, const int index) {
    asset_entry *entry = entry_at(kind, index);
    if (!entry) return 0;
    snprintf(selected_key(kind), MAX_BUFFER_SIZE, "%s", entry->key);
    return 1;
}

int wasabi_asset_select(const wasabi_asset_kind kind, const int index) {
    if (!wasabi_asset_preview(kind, index)) return 0;
    return 1;
}

static const char *collection_path(const wasabi_asset_kind kind) {
    if (kind == wasabi_asset_filter) return WASABI_FILTER_COLLECTION;
    if (kind == wasabi_asset_shader) return WASABI_SHADER_COLLECTION;
    return WASABI_OVERLAY_COLLECTION;
}

static int collection_has(const wasabi_asset_kind kind, const char *key) {
    FILE *file = fopen(collection_path(kind), "r");
    if (!file) return 0;
    char line[ASSET_KEY_MAX + 2];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, key) == 0) {
            found = 1;
            break;
        }
    }
    fclose(file);
    return found;
}

static int collection_toggle(const wasabi_asset_kind kind, const char *key) {
    char (*keys)[ASSET_KEY_MAX] = NULL;
    int count = 0;
    int capacity = 0;
    FILE *file = fopen(collection_path(kind), "r");
    char line[ASSET_KEY_MAX + 2];
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!line[0]) continue;
            if (count == capacity) {
                const int next_capacity = capacity ? capacity * 2 : 16;
                void *next = realloc(keys, (size_t) next_capacity * sizeof(*keys));
                if (!next) {
                    free(keys);
                    fclose(file);
                    return -1;
                }
                keys = next;
                capacity = next_capacity;
            }
            snprintf(keys[count++], ASSET_KEY_MAX, "%s", line);
        }
        fclose(file);
    }

    int found = -1;
    for (int index = 0; index < count; index++)
        if (strcmp(keys[index], key) == 0) {
            found = index;
            break;
        }
    const int added = found < 0;
    if (found >= 0) {
        memmove(keys + found, keys + found + 1, (size_t) (count - found - 1) * sizeof(*keys));
        count--;
    } else {
        if (count == capacity) {
            const int next_capacity = capacity ? capacity * 2 : 16;
            void *next = realloc(keys, (size_t) next_capacity * sizeof(*keys));
            if (!next) {
                free(keys);
                return -1;
            }
            keys = next;
            capacity = next_capacity;
        }
        snprintf(keys[count++], ASSET_KEY_MAX, "%s", key);
    }

    size_t size = 1;
    for (int index = 0; index < count; index++) size += strlen(keys[index]) + 1;
    char *text = malloc(size);
    if (!text) {
        free(keys);
        return -1;
    }
    size_t used = 0;
    for (int index = 0; index < count; index++)
        used += (size_t) snprintf(text + used, size - used, "%s\n", keys[index]);
    text[used] = '\0';
    create_directories(collection_path(kind), 1);
    const int okay = write_text_to_file_atomic(collection_path(kind), CHAR, text);
    free(text);
    free(keys);
    return okay ? added : -1;
}

static int browser_reserve(const int count) {
    if (count <= browser.capacity) return 1;
    void *next = realloc(browser.rows, (size_t) count * sizeof(*browser.rows));
    if (!next) return 0;
    browser.rows = next;
    browser.capacity = count;
    return 1;
}

static browser_row *browser_append(const wasabi_asset_row_type type) {
    if (browser.count >= browser.capacity) return NULL;
    browser_row *row = &browser.rows[browser.count++];
    memset(row, 0, sizeof(*row));
    row->type = type;
    row->item = -1;
    return row;
}

static int compare_rows(const void *left, const void *right) {
    const browser_row *a = left;
    const browser_row *b = right;
    const int folded = strcasecmp(a->label, b->label);
    return folded ? folded : strcmp(a->label, b->label);
}

static void collection_label(const asset_entry *entry, char *label, const size_t size) {
    const char *slash = strchr(entry->key, '/');
    if (!slash) {
        snprintf(label, size, "%s", entry->label);
        return;
    }
    snprintf(label, size, "%.*s - %s", (int) (slash - entry->key), entry->key, entry->label);
}

static int browser_build(void) {
    asset_store *store = &stores[browser.kind];
    if (!browser_reserve(store->count + 4)) return 0;
    browser.count = 0;
    if (!browser.collection && !browser.directory[0]) {
        browser_append(wasabi_asset_row_download);
        browser_append(wasabi_asset_row_collection);
        browser_row *none = browser_append(wasabi_asset_row_none);
        if (none) none->item = 0;
    }
    const int sortable = browser.count;
    for (int index = 1; index < store->count; index++) {
        asset_entry *entry = &store->entries[index];
        if (browser.collection) {
            if (!collection_has(browser.kind, entry->key)) continue;
            browser_row *row = browser_append(wasabi_asset_row_item);
            if (!row) return 0;
            row->item = index;
            snprintf(row->key, sizeof(row->key), "%s", entry->key);
            collection_label(entry, row->label, sizeof(row->label));
            continue;
        }

        const char *relative = entry->key;
        if (browser.directory[0]) {
            const size_t length = strlen(browser.directory);
            if (strncmp(entry->key, browser.directory, length) != 0 || entry->key[length] != '/') continue;
            relative = entry->key + length + 1;
        }
        const char *slash = strchr(relative, '/');
        if (!slash) {
            browser_row *row = browser_append(wasabi_asset_row_item);
            if (!row) return 0;
            row->item = index;
            snprintf(row->key, sizeof(row->key), "%s", entry->key);
            snprintf(row->label, sizeof(row->label), "%s", entry->label);
            continue;
        }
        char directory[ASSET_KEY_MAX];
        snprintf(directory, sizeof(directory), "%.*s", (int) (slash - relative), relative);
        int duplicate = 0;
        for (int row = sortable; row < browser.count; row++)
            if (browser.rows[row].type == wasabi_asset_row_directory
                && strcasecmp(browser.rows[row].label, directory) == 0)
                duplicate = 1;
        if (duplicate) continue;
        browser_row *row = browser_append(wasabi_asset_row_directory);
        if (!row) return 0;
        snprintf(row->key, sizeof(row->key), "%s", directory);
        snprintf(row->label, sizeof(row->label), "%s", directory);
    }
    if (browser.count > sortable + 1)
        qsort(browser.rows + sortable, (size_t) (browser.count - sortable), sizeof(*browser.rows), compare_rows);
    if (browser.count == 0) browser_append(wasabi_asset_row_empty);
    return 1;
}

void wasabi_asset_browser_open(const wasabi_asset_kind kind) {
    browser.kind = kind;
    browser.collection = 0;
    browser.directory[0] = '\0';
    wasabi_assets_refresh(kind);
    const char *selected = selected_key(kind);
    if (selected && strcasecmp(selected, "none") != 0) {
        const char *slash = strrchr(selected, '/');
        if (slash)
            snprintf(browser.directory, sizeof(browser.directory), "%.*s", (int) (slash - selected), selected);
    }
    browser_build();
}

int wasabi_asset_browser_count(void) { return browser.count; }

wasabi_asset_row_type wasabi_asset_browser_type(const int row) {
    return row >= 0 && row < browser.count ? browser.rows[row].type : wasabi_asset_row_empty;
}

const char *wasabi_asset_browser_label(const int row) {
    if (row < 0 || row >= browser.count) return "";
    const browser_row *entry = &browser.rows[row];
    switch (entry->type) {
        case wasabi_asset_row_download: return lang.muxretro.catalogue_screen.downloads;
        case wasabi_asset_row_collection: return lang.muxretro.catalogue_screen.collection;
        case wasabi_asset_row_none: return lang.generic.none;
        case wasabi_asset_row_empty: return lang.muxretro.catalogue_screen.collection_empty;
        default: return entry->label;
    }
}

const char *wasabi_asset_browser_glyph(const int row) {
    const wasabi_asset_row_type type = wasabi_asset_browser_type(row);
    if (type == wasabi_asset_row_download) return "download";
    if (type == wasabi_asset_row_collection || (type == wasabi_asset_row_empty && browser.collection)) return "star";
    if (type == wasabi_asset_row_directory || type == wasabi_asset_row_empty) return "folder";
    if (browser.kind == wasabi_asset_filter) return "filter";
    if (browser.kind == wasabi_asset_shader) return "shader";
    return "overlay";
}

int wasabi_asset_browser_item(const int row) {
    return row >= 0 && row < browser.count ? browser.rows[row].item : -1;
}

int wasabi_asset_browser_focus(void) {
    const int selected = wasabi_asset_selected(browser.kind);
    for (int row = 0; row < browser.count; row++)
        if (browser.rows[row].item == selected) return row;
    if (!browser.collection && !browser.directory[0]) {
        const char *key = wasabi_asset_key(browser.kind, selected);
        const char *separator = key ? strchr(key, '/') : NULL;
        if (separator) {
            const size_t length = (size_t) (separator - key);
            for (int row = 0; row < browser.count; row++)
                if (browser.rows[row].type == wasabi_asset_row_directory
                    && strlen(browser.rows[row].key) == length
                    && strncasecmp(browser.rows[row].key, key, length) == 0)
                    return row;
        }
    }
    return 0;
}

int wasabi_asset_browser_enter(const int row) {
    if (row < 0 || row >= browser.count) return 0;
    const browser_row *entry = &browser.rows[row];
    if (entry->type == wasabi_asset_row_collection) {
        browser.collection = 1;
        browser.directory[0] = '\0';
        browser_build();
        return 1;
    }
    if (entry->type == wasabi_asset_row_directory) {
        snprintf(browser.directory, sizeof(browser.directory), "%s", entry->key);
        browser_build();
        return 1;
    }
    return 0;
}

int wasabi_asset_browser_back(void) {
    if (browser.collection || browser.directory[0]) {
        browser.collection = 0;
        browser.directory[0] = '\0';
        browser_build();
        return 1;
    }
    return 0;
}

int wasabi_asset_browser_collected(const int row) {
    if (row < 0 || row >= browser.count || browser.rows[row].type != wasabi_asset_row_item) return 0;
    return collection_has(browser.kind, browser.rows[row].key);
}

int wasabi_asset_browser_toggle_collection(const int row) {
    if (row < 0 || row >= browser.count || browser.rows[row].type != wasabi_asset_row_item) return -1;
    const int result = collection_toggle(browser.kind, browser.rows[row].key);
    if (result >= 0 && browser.collection) {
        browser_build();
        if (browser.count == 1 && browser.rows[0].type == wasabi_asset_row_empty) {
            browser.collection = 0;
            browser.directory[0] = '\0';
            browser_build();
        }
    }
    return result;
}

int wasabi_asset_browser_removable(const int row) {
    if (browser.collection || row < 0 || row >= browser.count
        || browser.rows[row].type != wasabi_asset_row_item)
        return 0;
    const asset_entry *entry = entry_at(browser.kind, browser.rows[row].item);
    if (!entry || !entry->path[0]) return 0;
    const char *root = browser.kind == wasabi_asset_filter ? WASABI_FILTER_PATH
                       : browser.kind == wasabi_asset_shader ? WASABI_SHADER_PATH
                                                             : WASABI_OVERLAY_PATH;
    return strncmp(entry->path, root, strlen(root)) == 0;
}

int wasabi_asset_browser_delete(const int row) {
    if (!wasabi_asset_browser_removable(row)) return 0;
    char removed_key[ASSET_KEY_MAX];
    char path[PATH_MAX];
    snprintf(removed_key, sizeof(removed_key), "%s", browser.rows[row].key);
    const asset_entry *entry = entry_at(browser.kind, browser.rows[row].item);
    if (!entry) return 0;
    snprintf(path, sizeof(path), "%s", entry->path);
    if (unlink(path) != 0 && errno != ENOENT) return 0;
    char metadata[PATH_MAX];
    if ((size_t) snprintf(metadata, sizeof(metadata), "%s.name", path) < sizeof(metadata)) unlink(metadata);
    if (collection_has(browser.kind, removed_key)) collection_toggle(browser.kind, removed_key);
    if (strcasecmp(selected_key(browser.kind), removed_key) == 0) wasabi_asset_select(browser.kind, 0);
    wasabi_assets_refresh(browser.kind);
    browser_build();
    if (browser.directory[0] && browser.count == 1 && browser.rows[0].type == wasabi_asset_row_empty) {
        browser.directory[0] = '\0';
        browser_build();
    }
    return 1;
}
