#include "launch.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <common/base/options.h>
#include <common/base/strutil.h>
#include <common/storage/fileio.h>
#include <common/storage/union.h>

int launch_marker_path(const char *dir_path, char *out, const size_t out_size) {
    char rel[PATH_MAX];
    union_get_relative_path(dir_path, rel, sizeof(rel));

    const size_t root_len = strlen(MAIN_ROM_DIR);
    if (strncasecmp(rel, MAIN_ROM_DIR, root_len) != 0 || rel[root_len] != '/') return 0;

    const char *sub = rel + root_len;
    while (*sub == '/')
        sub++;
    if (!*sub) return 0;

    snprintf(out, out_size, INFO_LCH_PATH "/%s", sub);
    remove_double_slashes(out);

    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '/')
        out[--len] = '\0';

    if (len + 4 >= out_size) return 0;
    snprintf(out + len, out_size - len, ".cfg");

    return 1;
}

int folder_marked_launch(const char *dir_path, char *out_path, const size_t out_size) {
    char marker[MAX_BUFFER_SIZE];
    if (!launch_marker_path(dir_path, marker, sizeof(marker)) || !file_exist(marker)) return 0;

    char *line = read_line_char_from(marker, 1);
    if (!line) return 0;

    const char *name = str_trim(line);
    int ok = name && *name && !strchr(name, '/') && strcmp(name, ".") != 0 && strcmp(name, "..") != 0;

    if (ok) {
        snprintf(out_path, out_size, "%s/%s", dir_path, name);

        if (!file_exist(out_path)) {
            char real_path[MAX_BUFFER_SIZE];
            ok = union_resolve_to_real(out_path, real_path, sizeof(real_path)) && file_exist(real_path);
            if (ok) snprintf(out_path, out_size, "%s", real_path);
        }
    }

    free(line);

    return ok;
}

static int split_launch_path(const char *file_path, char *dir_path, const size_t dir_size, const char **file_name) {
    snprintf(dir_path, dir_size, "%s", file_path);

    char *slash = strrchr(dir_path, '/');
    if (!slash || slash == dir_path) return 0;

    *slash = '\0';
    *file_name = file_path + (slash - dir_path) + 1;

    return **file_name != '\0';
}

int launch_marker_matches(const char *file_path) {
    char dir_path[MAX_BUFFER_SIZE];
    const char *file_name = NULL;
    if (!split_launch_path(file_path, dir_path, sizeof(dir_path), &file_name)) return 0;

    char marked[MAX_BUFFER_SIZE];
    return folder_marked_launch(dir_path, marked, sizeof(marked)) && strcmp(marked, file_path) == 0;
}

int launch_marker_set(const char *file_path) {
    char dir_path[MAX_BUFFER_SIZE];
    const char *file_name = NULL;
    if (!split_launch_path(file_path, dir_path, sizeof(dir_path), &file_name)) return 0;

    char marker[MAX_BUFFER_SIZE];
    if (!launch_marker_path(dir_path, marker, sizeof(marker))) return 0;

    create_directories(marker, 1);
    write_text_to_file(marker, "w", CHAR, file_name);

    return launch_marker_matches(file_path);
}

int launch_marker_clear(const char *dir_path) {
    char marker[MAX_BUFFER_SIZE];
    if (!launch_marker_path(dir_path, marker, sizeof(marker))) return 1;

    return !file_exist(marker) || remove(marker) == 0;
}

int content_launch_folder_name(const char *file_path, char *out, const size_t out_size) {
    if (!file_path || !*file_path || !launch_marker_matches(file_path)) return 0;

    char dir_path[MAX_BUFFER_SIZE];
    snprintf(dir_path, sizeof(dir_path), "%s", file_path);

    char *slash = strrchr(dir_path, '/');
    if (slash) *slash = '\0';

    const char *folder = strrchr(dir_path, '/');
    snprintf(out, out_size, "%s", folder ? folder + 1 : dir_path);

    return 1;
}
