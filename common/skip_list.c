#include "skip_list.h"
#include "common.h"
#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_skiplist(SkipList *sl) {
    sl->items = NULL;
    sl->count = 0;
    sl->capacity = 0;
}

void free_skiplist(SkipList *sl) {
    for (size_t i = 0; i < sl->count; i++) {
        free(sl->items[i]);
    }

    free(sl->items);

    sl->items = NULL;
    sl->count = 0;
    sl->capacity = 0;
}

void add_to_skiplist(SkipList *sl, const char *dir, const char *name) {
    char full_path[MAX_BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, name);

    if (sl->count == sl->capacity) {
        size_t new_capacity = sl->capacity == 0 ? 8 : sl->capacity * 2;
        char **new_items = realloc(sl->items, new_capacity * sizeof(char*));

        if (!new_items) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }

        sl->items = new_items;
        sl->capacity = new_capacity;
    }

    sl->items[sl->count] = strdup(full_path);

    if (!sl->items[sl->count]) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }

    sl->count++;
}

bool in_skiplist(const SkipList *sl, const char *name) {
    for (size_t i = 0; i < sl->count; i++) {
        if (strcasecmp(sl->items[i], name) == 0) return true;
    }

    return false;
}

bool ends_with(char *str, const char *suffix) {
    if (!str || !suffix) return false;

    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);

    if (len_suffix > len_str) return false;
    return strcmp(str_tolower(str) + len_str - len_suffix, suffix) == 0;
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

        size_t len = end - start;
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

        size_t len = end - start;
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
