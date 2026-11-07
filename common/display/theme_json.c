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

struct json theme_json_lookup(const struct json scheme, const char *section, const char *key) {
    const struct json group = json_object_get(scheme, section);
    if (json_type(group) != JSON_OBJECT) return (struct json) {0};

    const struct json exact = json_object_get(group, key);
    if (json_exists(exact)) return exact;

    char name[MAX_BUFFER_SIZE];
    size_t length = 0;
    for (; key[length] && length < sizeof(name) - 1; length++) {
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
    const char *stripped = strncmp(name, prefix, skip) == 0 && name[skip] ? name + skip : name;
    return find_path(group, stripped);
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
