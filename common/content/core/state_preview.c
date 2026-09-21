#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "state_preview.h"

struct preview_candidate {
    char name[NAME_MAX + 1];
    long long created;
    struct timespec modified;
    int priority;
    int valid;
};

static int state_priority(const char *stem) {
    if (strcmp(stem, "quicksave") == 0) return 4;
    if (strncmp(stem, "slot_", 5) == 0) stem += 5;
    else if (strncmp(stem, "timeline_", 9) == 0) {
        stem += 9;
        if (!*stem) return 0;
        while (*stem)
            if (!isdigit((unsigned char) *stem++)) return 0;
        return 2;
    } else if (strcmp(stem, "autosave") == 0) {
        return 1;
    } else {
        return 0;
    }

    if (!*stem) return 0;
    while (*stem)
        if (!isdigit((unsigned char) *stem++)) return 0;
    return 3;
}

static int join_path(char *out, const size_t out_size, const char *directory, const char *name) {
    const int written = snprintf(out, out_size, "%s/%s", directory, name);
    return written > 0 && (size_t) written < out_size;
}

static int candidate_is_newer(const struct preview_candidate *candidate, const struct preview_candidate *best) {
    if (!best->valid) return 1;
    if (candidate->created != best->created) return candidate->created > best->created;
    if (candidate->priority != best->priority) return candidate->priority > best->priority;
    if (candidate->modified.tv_sec != best->modified.tv_sec)
        return candidate->modified.tv_sec > best->modified.tv_sec;
    if (candidate->modified.tv_nsec != best->modified.tv_nsec)
        return candidate->modified.tv_nsec > best->modified.tv_nsec;
    return strcmp(candidate->name, best->name) < 0;
}

static void consider_candidate(
    const char *directory, const char *stem, const long long created, struct preview_candidate *best
) {
    const int priority = state_priority(stem);
    if (!priority) return;

    char state_name[NAME_MAX + 1];
    char image_name[NAME_MAX + 1];
    const size_t stem_length = strlen(stem);
    if (stem_length + sizeof(".state") > sizeof(state_name) || stem_length + sizeof(".png") > sizeof(image_name))
        return;
    memcpy(state_name, stem, stem_length);
    memcpy(state_name + stem_length, ".state", sizeof(".state"));
    memcpy(image_name, stem, stem_length);
    memcpy(image_name + stem_length, ".png", sizeof(".png"));

    char state_path[PATH_MAX];
    char image_path[PATH_MAX];
    if (!join_path(state_path, sizeof(state_path), directory, state_name)
        || !join_path(image_path, sizeof(image_path), directory, image_name))
        return;

    struct stat state_info;
    struct stat image_info;
    if (stat(state_path, &state_info) != 0 || !S_ISREG(state_info.st_mode) || state_info.st_size <= 0
        || stat(image_path, &image_info) != 0 || !S_ISREG(image_info.st_mode) || image_info.st_size <= 0)
        return;

    const struct preview_candidate candidate = {
        .created = created > 0 ? created : (long long) state_info.st_mtim.tv_sec,
        .modified = state_info.st_mtim,
        .priority = priority,
        .valid = 1,
    };
    struct preview_candidate named = candidate;
    memcpy(named.name, image_name, stem_length + sizeof(".png"));
    if (candidate_is_newer(&named, best)) *best = named;
}

static char *trim(char *text) {
    while (isspace((unsigned char) *text))
        text++;

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1]))
        *--end = '\0';
    return text;
}

static void consider_manifest(const char *directory, struct preview_candidate *best) {
    char manifest_path[PATH_MAX];
    if (!join_path(manifest_path, sizeof(manifest_path), directory, "states.ini")) return;

    FILE *manifest = fopen(manifest_path, "r");
    if (!manifest) return;

    char group[NAME_MAX + 1] = "";
    long long created = 0;
    char line[1024];

    while (fgets(line, sizeof(line), manifest)) {
        char *text = trim(line);
        if (*text == '[') {
            if (*group) consider_candidate(directory, group, created, best);
            group[0] = '\0';
            created = 0;

            char *close = strchr(text + 1, ']');
            if (!close) continue;
            *close = '\0';
            const char *name = trim(text + 1);
            const size_t name_length = strlen(name);
            if (name_length >= sizeof(group)) continue;
            memcpy(group, name, name_length + 1);
            continue;
        }

        if (!*group) continue;
        char *separator = strchr(text, '=');
        if (!separator) continue;
        *separator = '\0';
        if (strcmp(trim(text), "created") == 0) created = strtoll(trim(separator + 1), NULL, 10);
    }

    if (*group) consider_candidate(directory, group, created, best);
    fclose(manifest);
}

int state_preview_latest(const char *directory, char *out, const size_t out_size) {
    if (!directory || !*directory || !out || !out_size) return 0;
    out[0] = '\0';

    struct preview_candidate best = {0};
    consider_manifest(directory, &best);
    if (best.valid) {
        const int written = snprintf(out, out_size, "%s", best.name);
        return written > 0 && (size_t) written < out_size;
    }

    DIR *dir = opendir(directory);
    if (!dir) return 0;

    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        const size_t length = strlen(entry->d_name);
        if (length <= 6 || strcasecmp(entry->d_name + length - 6, ".state") != 0) continue;

        char stem[NAME_MAX + 1];
        if (length - 6 >= sizeof(stem)) continue;
        memcpy(stem, entry->d_name, length - 6);
        stem[length - 6] = '\0';
        consider_candidate(directory, stem, 0, &best);
    }
    closedir(dir);

    if (!best.valid) return 0;
    const int written = snprintf(out, out_size, "%s", best.name);
    return written > 0 && (size_t) written < out_size;
}
