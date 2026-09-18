#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/storage/fileio.h>
#include "preset_browser.h"
#include "../core/paths.h"

typedef struct {
    int loaded;
    int count;
    int capacity;
    char (*keys)[PRESET_BROWSER_KEY_MAX];
} collection_store;

static collection_store collections[preset_browser_kind_count];

static const char *collection_path(const enum preset_browser_kind kind) {
    switch (kind) {
        case preset_browser_filter:
            return RETRO_SHARE_PATH "filter-collection";
        case preset_browser_shader:
            return RETRO_SHARE_PATH "shader-collection";
        case preset_browser_overlay:
            return RETRO_SHARE_PATH "overlay-collection";
        default:
            return "";
    }
}

static int key_valid(const char *key) {
    if (!key || !*key || key[0] == '/' || strlen(key) >= PRESET_BROWSER_KEY_MAX || strcasecmp(key, "none") == 0)
        return 0;

    const char *segment = key;
    int separators = 0;
    for (const unsigned char *cursor = (const unsigned char *) key;; cursor++) {
        if (*cursor != '\0' && (*cursor < 0x20 || *cursor == 0x7f || *cursor == '\\')) return 0;
        if (*cursor != '/' && *cursor != '\0') continue;

        const size_t length = (size_t) ((const char *) cursor - segment);
        if (length == 0 || (length == 1 && segment[0] == '.')
            || (length == 2 && segment[0] == '.' && segment[1] == '.'))
            return 0;
        if (*cursor == '\0') break;
        if (++separators > 1) return 0;
        segment = (const char *) cursor + 1;
    }

    return 1;
}

static int store_append(collection_store *store, const char *key) {
    if (store->count == store->capacity) {
        const int capacity = store->capacity ? store->capacity * 2 : 32;
        void *next = realloc(store->keys, (size_t) capacity * sizeof(*store->keys));
        if (!next) return 0;
        store->keys = next;
        store->capacity = capacity;
    }

    snprintf(store->keys[store->count], sizeof(store->keys[store->count]), "%s", key);
    store->count++;
    return 1;
}

static void store_load(const enum preset_browser_kind kind) {
    if (kind < 0 || kind >= preset_browser_kind_count) return;

    collection_store *store = &collections[kind];
    if (store->loaded) return;
    store->loaded = 1;

    FILE *file = fopen(collection_path(kind), "r");
    if (!file) return;

    char line[PRESET_BROWSER_KEY_MAX + 2];
    while (fgets(line, sizeof(line), file)) {
        const int complete = strchr(line, '\n') != NULL || feof(file);
        line[strcspn(line, "\r\n")] = '\0';
        if (!complete || !key_valid(line)) continue;

        int duplicate = 0;
        for (int index = 0; index < store->count && !duplicate; index++)
            duplicate = strcmp(store->keys[index], line) == 0;
        if (!duplicate && !store_append(store, line)) break;
    }

    fclose(file);
}

static int store_write(const enum preset_browser_kind kind) {
    collection_store *store = &collections[kind];
    size_t length = 1;
    for (int index = 0; index < store->count; index++)
        length += strlen(store->keys[index]) + 1;

    char *text = malloc(length);
    if (!text) return 0;

    size_t used = 0;
    for (int index = 0; index < store->count; index++) {
        const size_t key_length = strlen(store->keys[index]);
        memcpy(text + used, store->keys[index], key_length);
        used += key_length;
        text[used++] = '\n';
    }
    text[used] = '\0';

    const char *path = collection_path(kind);
    create_directories(path, 1);
    const int written = write_text_to_file_atomic(path, CHAR, text);
    free(text);
    return written;
}

int preset_browser_is_collected(const enum preset_browser_kind kind, const char *key) {
    if (kind < 0 || kind >= preset_browser_kind_count || !key_valid(key)) return 0;
    store_load(kind);

    const collection_store *store = &collections[kind];
    for (int index = 0; index < store->count; index++)
        if (strcmp(store->keys[index], key) == 0) return 1;
    return 0;
}

int preset_browser_toggle_collection(const enum preset_browser_kind kind, const char *key) {
    if (kind < 0 || kind >= preset_browser_kind_count || !key_valid(key)) return -1;
    store_load(kind);

    collection_store *store = &collections[kind];
    for (int index = 0; index < store->count; index++) {
        if (strcmp(store->keys[index], key) != 0) continue;
        memmove(
            store->keys + index, store->keys + index + 1, (size_t) (store->count - index - 1) * sizeof(*store->keys)
        );
        store->count--;
        if (store_write(kind)) return 0;
        store_append(store, key);
        return -1;
    }

    if (!store_append(store, key)) return -1;
    if (store_write(kind)) return 1;
    store->count--;
    return -1;
}

void preset_browser_remove_collection(const enum preset_browser_kind kind, const char *key) {
    if (preset_browser_is_collected(kind, key)) preset_browser_toggle_collection(kind, key);
}

static int row_compare(const void *left, const void *right) {
    const preset_browser_row *a = left;
    const preset_browser_row *b = right;
    const int folded = strcasecmp(a->label, b->label);
    if (folded) return folded;
    const int exact = strcmp(a->label, b->label);
    if (exact) return exact;
    return strcmp(a->key, b->key);
}

static int ensure_rows(preset_browser *browser, const int count) {
    if (count <= browser->row_capacity) return 1;
    void *next = realloc(browser->rows, (size_t) count * sizeof(*browser->rows));
    if (!next) return 0;
    browser->rows = next;
    browser->row_capacity = count;
    return 1;
}

static preset_browser_row *append_row(preset_browser *browser, const enum preset_browser_row_type type) {
    if (browser->row_count >= browser->row_capacity) return NULL;
    preset_browser_row *row = &browser->rows[browser->row_count++];
    memset(row, 0, sizeof(*row));
    row->type = type;
    row->item_index = -1;
    return row;
}

static void collection_label(const char *key, const char *label, char *output, const size_t output_size) {
    const char *slash = strrchr(key, '/');
    if (!slash) {
        snprintf(output, output_size, "%s", label);
        return;
    }

    size_t used = 0;
    for (const char *cursor = key; cursor < slash && used + 1 < output_size; cursor++) {
        if (*cursor == '/') {
            const char separator[] = " - ";
            for (size_t index = 0; index < sizeof(separator) - 1 && used + 1 < output_size; index++)
                output[used++] = separator[index];
        } else {
            output[used++] = *cursor;
        }
    }
    output[used] = '\0';
    if (used + 1 < output_size) snprintf(output + used, output_size - used, " - %s", label);
}

static int build_rows(preset_browser *browser) {
    if (!ensure_rows(browser, browser->item_count + 4)) return 0;
    browser->row_count = 0;

    if (!browser->collection && !browser->directory[0]) {
        append_row(browser, preset_browser_row_download);
        snprintf(append_row(browser, preset_browser_row_collection)->key, PRESET_BROWSER_KEY_MAX, "%s", "collection");
        append_row(browser, preset_browser_row_none)->item_index = 0;
    }

    const int sortable_start = browser->row_count;
    const size_t prefix_length = strlen(browser->directory);

    for (int index = 1; index < browser->item_count; index++) {
        const char *key = browser->key_fn(index);
        const char *label = browser->label_fn(index);
        if (!key_valid(key)) continue;

        if (browser->collection) {
            if (!preset_browser_is_collected(browser->kind, key)) continue;
            preset_browser_row *row = append_row(browser, preset_browser_row_item);
            if (!row) return 0;
            row->item_index = index;
            snprintf(row->key, sizeof(row->key), "%s", key);
            collection_label(key, label, row->label, sizeof(row->label));
            continue;
        }

        const char *relative = key;
        if (prefix_length) {
            if (strncmp(key, browser->directory, prefix_length) != 0 || key[prefix_length] != '/') continue;
            relative = key + prefix_length + 1;
        }

        const char *slash = strchr(relative, '/');
        if (!slash) {
            preset_browser_row *row = append_row(browser, preset_browser_row_item);
            if (!row) return 0;
            row->item_index = index;
            snprintf(row->key, sizeof(row->key), "%s", key);
            snprintf(row->label, sizeof(row->label), "%s", label);
            continue;
        }

        char child[PRESET_BROWSER_KEY_MAX];
        const size_t child_length = (size_t) (slash - relative);
        if (!child_length || child_length >= sizeof(child)) continue;
        snprintf(child, sizeof(child), "%.*s", (int) child_length, relative);

        char child_key[PRESET_BROWSER_KEY_MAX];
        const int child_key_length = prefix_length
                                         ? snprintf(child_key, sizeof(child_key), "%s/%s", browser->directory, child)
                                         : snprintf(child_key, sizeof(child_key), "%s", child);
        if (child_key_length < 0 || (size_t) child_key_length >= sizeof(child_key)) continue;

        int seen = 0;
        for (int row_index = sortable_start; row_index < browser->row_count && !seen; row_index++)
            seen = browser->rows[row_index].type == preset_browser_row_directory
                   && strcmp(browser->rows[row_index].key, child_key) == 0;
        if (seen) continue;

        preset_browser_row *row = append_row(browser, preset_browser_row_directory);
        if (!row) return 0;
        snprintf(row->key, sizeof(row->key), "%s", child_key);
        snprintf(row->label, sizeof(row->label), "%s", child);
    }

    if (browser->row_count > sortable_start + 1)
        qsort(
            browser->rows + sortable_start, (size_t) (browser->row_count - sortable_start), sizeof(*browser->rows),
            row_compare
        );

    if (browser->row_count == 0) append_row(browser, preset_browser_row_empty);
    return 1;
}

void preset_browser_init(preset_browser *browser, const enum preset_browser_kind kind) {
    memset(browser, 0, sizeof(*browser));
    browser->kind = kind;
}

void preset_browser_destroy(preset_browser *browser) {
    free(browser->rows);
    memset(browser, 0, sizeof(*browser));
}

int preset_browser_configure(
    preset_browser *browser, const int item_count, const preset_browser_text_fn key_fn,
    const preset_browser_text_fn label_fn
) {
    if (!browser || item_count < 1 || !key_fn || !label_fn) return 0;
    browser->item_count = item_count;
    browser->key_fn = key_fn;
    browser->label_fn = label_fn;
    if (!build_rows(browser)) return 0;
    if (browser->directory[0] && browser->row_count == 1 && browser->rows[0].type == preset_browser_row_empty) {
        browser->directory[0] = '\0';
        return build_rows(browser);
    }
    return 1;
}

int preset_browser_open_root(preset_browser *browser) {
    browser->directory[0] = '\0';
    browser->collection = 0;
    return build_rows(browser);
}

int preset_browser_focus_key(preset_browser *browser, const char *key) {
    browser->collection = 0;
    browser->directory[0] = '\0';
    if (key && strcasecmp(key, "none") != 0) {
        const char *slash = strrchr(key, '/');
        if (slash) snprintf(browser->directory, sizeof(browser->directory), "%.*s", (int) (slash - key), key);
    }
    return build_rows(browser);
}

int preset_browser_enter(preset_browser *browser, const int row_index) {
    const preset_browser_row *row = preset_browser_row_at(browser, row_index);
    if (!row) return 0;
    if (row->type == preset_browser_row_collection) {
        browser->collection = 1;
        browser->directory[0] = '\0';
        return build_rows(browser);
    }
    if (row->type != preset_browser_row_directory) return 0;
    browser->collection = 0;
    snprintf(browser->directory, sizeof(browser->directory), "%s", row->key);
    return build_rows(browser);
}

int preset_browser_back(preset_browser *browser, char *return_key, const int return_key_size) {
    if (return_key && return_key_size > 0) return_key[0] = '\0';
    if (browser->collection) {
        browser->collection = 0;
        if (return_key && return_key_size > 0) snprintf(return_key, (size_t) return_key_size, "collection");
        return build_rows(browser);
    }
    if (!browser->directory[0]) return 0;

    char previous[PRESET_BROWSER_KEY_MAX];
    snprintf(previous, sizeof(previous), "%s", browser->directory);
    char *slash = strrchr(browser->directory, '/');
    if (slash)
        *slash = '\0';
    else
        browser->directory[0] = '\0';
    if (return_key && return_key_size > 0) snprintf(return_key, (size_t) return_key_size, "%s", previous);
    return build_rows(browser);
}

int preset_browser_find_row(const preset_browser *browser, const enum preset_browser_row_type type, const char *key) {
    for (int index = 0; index < browser->row_count; index++) {
        const preset_browser_row *row = &browser->rows[index];
        if (row->type != type) continue;
        if (!key || strcmp(row->key, key) == 0) return index;
    }
    return 0;
}

const preset_browser_row *preset_browser_row_at(const preset_browser *browser, const int row_index) {
    return browser && row_index >= 0 && row_index < browser->row_count ? &browser->rows[row_index] : NULL;
}
