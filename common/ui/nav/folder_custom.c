#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <json/json.h>
#include <common/base/strutil.h>
#include <common/runtime/init.h>
#include <common/storage/fileio.h>
#include <common/storage/union.h>
#include <common/display/colour_scheme.h>
#include <common/display/theme_json.h>
#include <common/ui/nav/content_layout.h>
#include <common/ui/nav/folder_custom.h>

#define FOLDER_JSON_SIZE 2048

static char folder_scheme[MAX_BUFFER_SIZE];
static char folder_colours[colour_role_count][COLOUR_VALUE_SIZE];

int folder_custom_path(const char *dir, char *out, const size_t size) {
    char rel[PATH_MAX];
    union_get_relative_path(dir, rel, sizeof(rel));

    const size_t root_len = strlen(MAIN_ROM_DIR);
    if (strncasecmp(rel, MAIN_ROM_DIR, root_len) != 0 || (rel[root_len] != '/' && rel[root_len] != '\0')) return 0;

    const char *sub = rel + root_len;
    while (*sub == '/')
        sub++;

    snprintf(out, size, FOLDER_CUSTOM_PATH "/%s", *sub ? sub : ".root");
    remove_double_slashes(out);

    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '/')
        out[--len] = '\0';

    if (len + 5 >= size) return 0;
    snprintf(out + len, size - len, ".json");

    return 1;
}

void folder_custom_set_scheme(const char *scheme) {
    if (scheme && strcmp(scheme, folder_scheme) == 0) return;

    folder_scheme[0] = '\0';
    memset(folder_colours, 0, sizeof(folder_colours));

    char path[PATH_MAX];
    if (!scheme || !*scheme || !colour_scheme_path(scheme, path, sizeof(path))) return;
    if (colour_scheme_read(path, folder_colours)) snprintf(folder_scheme, sizeof(folder_scheme), "%s", scheme);
}

const char *folder_custom_scheme(void) {
    return folder_scheme;
}

const char *folder_custom_colour(const int role) {
    if (!folder_scheme[0] || role < 0 || role >= colour_role_count) return NULL;
    if (strcmp(mux_module, "muxplore") != 0) return NULL;

    return folder_colours[role];
}

int folder_custom_load(const char *dir) {
    const content_layout_config *before_layout = content_layout_current(&config);
    content_layout_config before = {0};
    if (before_layout) before = *before_layout;

    char scheme_before[MAX_BUFFER_SIZE];
    snprintf(scheme_before, sizeof(scheme_before), "%s", folder_scheme);

    content_layout_folder_use(0);
    char scheme[MAX_BUFFER_SIZE] = "";

    char path[PATH_MAX];
    char *data = dir && folder_custom_path(dir, path, sizeof(path)) ? theme_json_read_file(path) : NULL;
    if (data) {
        const struct json root = json_parse(data);
        const struct json layout = json_object_get(root, "layout");
        if (json_type(layout) == JSON_OBJECT) {
            content_layout_config *own = content_layout_folder_config();
            *own = config.settings.layout[content_layout_explore];
            content_layout_read(layout, own);
            content_layout_folder_use(1);
        }

        const struct json colour = json_object_get(root, "colour");
        if (json_type(colour) == JSON_STRING) json_string_copy(colour, scheme, sizeof(scheme));

        free(data);
    }

    folder_custom_set_scheme(scheme);

    const content_layout_config *after = content_layout_current(&config);
    int changed = after && memcmp(&before, after, sizeof(before)) != 0 ? folder_changed_layout : 0;
    if (strcmp(scheme_before, folder_scheme) != 0) changed |= folder_changed_colour;

    return changed;
}

static void prune_folders(char *path) {
    const size_t root = strlen(FOLDER_CUSTOM_PATH);
    char *slash;

    while ((slash = strrchr(path, '/')) && (size_t) (slash - path) > root) {
        *slash = '\0';
        if (rmdir(path) != 0) return;
    }

    rmdir(FOLDER_CUSTOM_PATH);
}

int folder_custom_save(const char *dir, const int own_layout) {
    char path[PATH_MAX];
    if (!folder_custom_path(dir, path, sizeof(path))) return 0;

    char fields[FOLDER_JSON_SIZE] = "";
    const content_layout_config *explore = &config.settings.layout[content_layout_explore];
    const size_t used =
        own_layout ? content_layout_write(fields, sizeof(fields), content_layout_folder_config(), explore) : 0;
    if (used >= sizeof(fields)) return 0;

    char scheme[MAX_BUFFER_SIZE] = "";
    if (folder_scheme[0] && json_escape(folder_scheme, scheme, sizeof(scheme)) >= sizeof(scheme)) return 0;

    char document[FOLDER_JSON_SIZE + MAX_BUFFER_SIZE];
    size_t length = (size_t) snprintf(document, sizeof(document), "{");
    if (used) length += (size_t) snprintf(document + length, sizeof(document) - length, "\"layout\":{%s}", fields);
    if (scheme[0]) {
        length +=
            (size_t) snprintf(document + length, sizeof(document) - length, "%s\"colour\":%s", used ? "," : "", scheme);
    }
    snprintf(document + length, sizeof(document) - length, "}");

    const int empty = !used && !scheme[0];
    const int saved = content_layout_store(path, document, empty);
    if (empty) prune_folders(path);

    return saved;
}

static int count_saved(const char *dir) {
    DIR *folder = opendir(dir);
    if (!folder) return 0;

    int count = 0;
    const struct dirent *entry;
    while ((entry = readdir(folder))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        if (dir_exist(path)) {
            count += count_saved(path);
        } else {
            const char *dot = strrchr(entry->d_name, '.');
            count += dot && strcasecmp(dot, ".json") == 0;
        }
    }

    closedir(folder);
    return count;
}

int folder_custom_count(void) {
    return count_saved(FOLDER_CUSTOM_PATH);
}

int folder_custom_reset_all(void) {
    folder_custom_set_scheme(NULL);
    content_layout_folder_use(0);
    return !dir_exist(FOLDER_CUSTOM_PATH) || remove_directory_recursive(FOLDER_CUSTOM_PATH) == 0;
}
