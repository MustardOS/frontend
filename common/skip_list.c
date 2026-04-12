#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "skip_list.h"
#include "common.h"
#include "options.h"

static size_t skip_hash(const char *key, size_t cap) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *) key; *p; p++) {
        h = (h ^ (unsigned char) *p) * 16777619u;
    }

    return (size_t) h & (cap - 1);
}

void init_skiplist(SkipList *sl) {
    if (!sl) return;

    sl->items = NULL;
    sl->count = 0;
    sl->capacity = 0;

    sl->bucket_cap = 512;
    sl->buckets = calloc(sl->bucket_cap, sizeof(char *));
}

void free_skiplist(SkipList *sl) {
    if (!sl) return;

    for (size_t i = 0; i < sl->count; i++) {
        free(sl->items[i]);
    }

    free(sl->items);
    free(sl->buckets);

    sl->items = NULL;
    sl->buckets = NULL;
    sl->count = 0;
    sl->capacity = 0;
    sl->bucket_cap = 0;
}

void add_to_skiplist(SkipList *sl, const char *dir, const char *name) {
    if (!sl || !dir || !name) return;

    char full_path[MAX_BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

    size_t i = skip_hash(full_path, sl->bucket_cap);

    while (sl->buckets[i]) {
        if (strcasecmp(sl->buckets[i], full_path) == 0) return;
        i = (i + 1) & (sl->bucket_cap - 1);
    }

    if (sl->count == sl->capacity) {
        size_t new_cap = sl->capacity ? sl->capacity * 2 : 256;
        char **tmp = realloc(sl->items, new_cap * sizeof(char *));
        if (!tmp) return;

        sl->items = tmp;
        sl->capacity = new_cap;
    }

    char *copy = strdup(full_path);
    if (!copy) return;

    sl->items[sl->count++] = copy;
    sl->buckets[i] = copy;
}

bool in_skiplist(const SkipList *sl, const char *name) {
    if (!sl || !sl->buckets || !name) return false;

    size_t i = skip_hash(name, sl->bucket_cap);

    while (sl->buckets[i]) {
        if (strcasecmp(sl->buckets[i], name) == 0) return true;
        i = (i + 1) & (sl->bucket_cap - 1);
    }

    return false;
}

bool ends_with(char *str, const char *suffix) {
    if (!str || !suffix) return false;

    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);

    if (len_suffix > len_str) return false;
    return strcasecmp(str + len_str - len_suffix, suffix) == 0;
}

void process_cue_file(char *dir, const char *filename, SkipList *sl) {
    char full_path[MAX_BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        perror(full_path);
        return;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        size_t skip_whitespace = 0;
        while (line[skip_whitespace] == ' ' || line[skip_whitespace] == '\t') skip_whitespace++;

        if (strncmp(line + skip_whitespace, "FILE", 4) != 0) continue;

        char *start = strchr(line + skip_whitespace, '"');
        if (!start) continue;
        start++;

        char *end = strchr(start, '"');
        if (!end) continue;

        size_t len = (size_t) (end - start);
        if (len > 0) {
            char *name = malloc(len + 1);
            if (!name) {
                perror("malloc");
                fclose(f);
                exit(EXIT_FAILURE);
            }
            strncpy(name, start, len);
            name[len] = '\0';

            add_to_skiplist(sl, dir, name);
            free(name);
        }
    }

    fclose(f);
}

void process_gdi_file(char *dir, const char *filename, SkipList *sl) {
    char full_path[MAX_BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        perror(full_path);
        return;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        char *start = strchr(line, '"');
        if (!start) continue;
        start++;

        char *end = strchr(start, '"');
        if (!end) continue;

        size_t len = (size_t) (end - start);
        if (len > 0) {
            char *name = malloc(len + 1);
            if (!name) {
                perror("malloc");
                fclose(f);
                exit(EXIT_FAILURE);
            }
            strncpy(name, start, len);
            name[len] = '\0';

            add_to_skiplist(sl, dir, name);
            free(name);
        }
    }

    fclose(f);
}

void process_m3u_file(char *dir, const char *filename, SkipList *sl) {
    char full_path[MAX_BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        perror(full_path);
        return;
    }

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0') add_to_skiplist(sl, dir, line);
    }

    fclose(f);
}
