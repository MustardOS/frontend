#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/base/strutil.h>
#include <common/display/colour_scheme.h>
#include <common/storage/fileio.h>

int colour_scheme_read(const char *path, char values[colour_role_count][COLOUR_VALUE_SIZE]) {
    memset(values, 0, sizeof(char[colour_role_count][COLOUR_VALUE_SIZE]));

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        if (!equals) continue;

        *equals = '\0';
        const char *key = str_trim(line);
        const char *value = str_trim(equals + 1);

        for (int role = 0; role < colour_role_count; role++) {
            if (strcmp(key, colour_role_keys[role]) == 0) snprintf(values[role], COLOUR_VALUE_SIZE, "%s", value);
        }
    }

    fclose(file);
    return 1;
}

static int scheme_compare(const void *a, const void *b) {
    return strcasecmp(((const colour_scheme *) a)->name, ((const colour_scheme *) b)->name);
}

int colour_scheme_scan(const char *dir, const int user, const int folders, colour_scheme *out, const int max) {
    DIR *folder = opendir(dir);
    if (!folder) return 0;

    int count = 0;
    const struct dirent *entry;
    while ((entry = readdir(folder)) && count < max) {
        if (entry->d_name[0] == '.') continue;

        colour_scheme *scheme = &out[count];
        snprintf(scheme->path, sizeof(scheme->path), "%s/%s", dir, entry->d_name);

        const int is_folder = dir_exist(scheme->path);
        const char *dot = strrchr(entry->d_name, '.');
        if (is_folder != folders || (!is_folder && (!dot || strcasecmp(dot, ".ini") != 0))) continue;

        const int length = is_folder ? (int) strlen(entry->d_name) : (int) (dot - entry->d_name);
        snprintf(scheme->name, sizeof(scheme->name), "%.*s", length, entry->d_name);
        scheme->user = user;
        scheme->folder = is_folder;
        count++;
    }

    closedir(folder);
    qsort(out, (size_t) count, sizeof(*out), scheme_compare);
    return count;
}

static int name_known(char names[][MAX_BUFFER_SIZE], const int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) return 1;
    }
    return 0;
}

static int
add_names(char names[][MAX_BUFFER_SIZE], int count, const int max, const char *root, const char *sub, const int user) {
    char dir[MAX_BUFFER_SIZE];
    snprintf(dir, sizeof(dir), "%s%s%s", root, *sub ? "/" : "", sub);

    colour_scheme found[32];
    const int files = colour_scheme_scan(dir, user, 0, found, (int) A_SIZE(found));
    for (int i = 0; i < files && count < max; i++) {
        char name[MAX_BUFFER_SIZE];
        snprintf(name, sizeof(name), "%s%s%s", sub, *sub ? "/" : "", found[i].name);
        if (!name_known(names, count, name)) snprintf(names[count++], MAX_BUFFER_SIZE, "%s", name);
    }

    if (*sub) return count;

    const int folders = colour_scheme_scan(dir, user, 1, found, (int) A_SIZE(found));
    for (int i = 0; i < folders && count < max; i++)
        count = add_names(names, count, max, root, found[i].name, user);

    return count;
}

int colour_scheme_names(char names[][MAX_BUFFER_SIZE], const int max) {
    const int count = add_names(names, 0, max, COLOUR_SCHEME_USER, "", 1);
    return add_names(names, count, max, COLOUR_SCHEME_PRESETS, "", 0);
}

int colour_scheme_path(const char *name, char *path, const size_t size) {
    const char *roots[] = {COLOUR_SCHEME_USER, COLOUR_SCHEME_PRESETS};

    for (size_t i = 0; i < A_SIZE(roots); i++) {
        snprintf(path, size, "%s/%s.ini", roots[i], name);
        if (file_exist(path)) return 1;
    }

    return 0;
}
