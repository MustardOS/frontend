#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/base/options.h>
#include <common/display/theme_json.h>
#include <common/runtime/init.h>
#include <common/storage/fileio.h>

static const struct {
    const char *section;
    const char *prefix;
} key_prefixes[] = {
    {"navigation", "nav_"},   {"keyboard", "osk_"},  {"notification", "msg_"},
    {"charging", "charger_"}, {"date", "datetime_"},
};

char *theme_json_read_file(const char *path) {
    char *data = file_exist(path) ? read_all_char_from(path) : NULL;
    if (data && json_valid(data)) return data;

    free(data);
    return NULL;
}

char *theme_json_read(const char *base) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/%stheme.json", base, mux_dim);
    if (file_exist(path)) return theme_json_read_file(path);

    snprintf(path, sizeof(path), "%s/theme.json", base);
    return theme_json_read_file(path);
}

struct json theme_json_module(const struct json root, const char *module, const char *part) {
    return json_object_get(json_object_get(json_object_get(root, "modules"), module), part);
}

static struct json find_path(const struct json node, const char *name) {
    const struct json direct = json_object_get(node, name);
    if (json_exists(direct)) return direct;

    for (const char *split = strchr(name, '_'); split; split = strchr(split + 1, '_')) {
        const struct json child = json_object_getn(node, name, (size_t) (split - name));
        if (json_type(child) != JSON_OBJECT) continue;

        const struct json found = find_path(child, split + 1);
        if (json_exists(found)) return found;
    }

    return (struct json) {0};
}

static const char *stripped_key(const char *section, const char *key, char *name, const size_t size) {
    size_t length = 0;
    for (; key[length] && length < size - 1; length++) {
        name[length] = (char) tolower((unsigned char) key[length]);
    }
    name[length] = '\0';

    char prefix[MAX_BUFFER_SIZE];
    snprintf(prefix, sizeof(prefix), "%s_", section);
    for (size_t i = 0; i < sizeof(key_prefixes) / sizeof(key_prefixes[0]); i++) {
        if (strcmp(key_prefixes[i].section, section) != 0) continue;
        snprintf(prefix, sizeof(prefix), "%s", key_prefixes[i].prefix);
    }

    const size_t skip = strlen(prefix);
    return strncmp(name, prefix, skip) == 0 && name[skip] ? name + skip : name;
}

struct json theme_json_lookup(const struct json scheme, const char *section, const char *key) {
    const struct json group = json_object_get(scheme, section);
    if (json_type(group) != JSON_OBJECT) return (struct json) {0};

    const struct json exact = json_object_get(group, key);
    if (json_exists(exact)) return exact;

    char name[MAX_BUFFER_SIZE];
    return find_path(group, stripped_key(section, key, name, sizeof(name)));
}

#define INDEX_NAME_MAX  80
#define INDEX_ENTRY_MAX 512

typedef struct {
    char name[INDEX_NAME_MAX];
    int depth;
    struct json value;
} index_entry;

static index_entry index_entries[INDEX_ENTRY_MAX];
static int index_count = 0;

static void index_flatten(const struct json node, char *path, const size_t length, const int depth) {
    for (struct json key = json_first(node); json_exists(key); key = json_next(json_next(key))) {
        const struct json value = json_next(key);

        const size_t at = length ? length + 1 : 0;
        if (at >= INDEX_NAME_MAX - 1) continue;

        if (length) path[length] = '_';
        json_string_copy(key, path + at, INDEX_NAME_MAX - at);
        const size_t end = strlen(path);

        if (json_type(value) == JSON_OBJECT) {
            index_flatten(value, path, end, depth + 1);
        } else if (index_count < INDEX_ENTRY_MAX) {
            index_entry *entry = &index_entries[index_count++];
            memcpy(entry->name, path, end + 1);
            entry->depth = depth;
            entry->value = value;
        }

        path[length] = '\0';
    }
}

static int index_compare(const void *left, const void *right) {
    const index_entry *a = left;
    const index_entry *b = right;
    const int order = strcmp(a->name, b->name);
    return order ? order : a->depth - b->depth;
}

static const index_entry *index_find(const char *name) {
    int low = 0;
    int high = index_count - 1;

    while (low <= high) {
        const int middle = (low + high) / 2;
        const int order = strcmp(index_entries[middle].name, name);
        if (order < 0) {
            low = middle + 1;
        } else if (order > 0) {
            high = middle - 1;
        } else {
            int first = middle;
            while (first > 0 && strcmp(index_entries[first - 1].name, name) == 0) {
                first--;
            }
            return &index_entries[first];
        }
    }

    return NULL;
}

int theme_json_index_group(const struct json scheme, const char *section) {
    index_count = 0;

    const struct json group = json_object_get(scheme, section);
    if (json_type(group) != JSON_OBJECT) return 0;

    char path[INDEX_NAME_MAX] = "";
    index_flatten(group, path, 0, 0);
    qsort(index_entries, index_count, sizeof(index_entries[0]), index_compare);

    return index_count;
}

struct json theme_json_index_lookup(const char *section, const char *key) {
    const index_entry *exact = index_find(key);
    if (exact && exact->depth == 0) return exact->value;

    char name[MAX_BUFFER_SIZE];
    const index_entry *entry = index_find(stripped_key(section, key, name, sizeof(name)));
    return entry ? entry->value : (struct json) {0};
}

long theme_json_number(const struct json value, const int base) {
    if (json_type(value) != JSON_STRING) return json_int(value);

    char text[64];
    json_string_copy(value, text, sizeof(text));
    const char *digits = text;
    if (*digits == '#') digits++;
    if (base == 16 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    return strtol(digits, NULL, base);
}

char *theme_json_module_layout(const char *base, const char *module) {
    char *data = theme_json_read(base);
    if (!data) return NULL;

    const struct json layout = theme_json_module(json_parse(data), module, "layout");
    char *copy = NULL;
    if (json_type(layout) == JSON_OBJECT) {
        const size_t length = json_raw_length(layout);
        copy = malloc(length + 1);
        if (copy) {
            memcpy(copy, json_raw(layout), length);
            copy[length] = '\0';
        }
    }

    free(data);
    return copy;
}
