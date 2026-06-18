#pragma once

#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;

    char **buckets;
    size_t bucket_cap;
} SkipList;

void init_skiplist(SkipList *sl);

void free_skiplist(SkipList *sl);

void add_to_skiplist(SkipList *sl, const char *dir, const char *name);

int in_skiplist(const SkipList *sl, const char *name);

int ends_with(char *str, const char *suffix);

void process_gdi_file(char *dir, const char *filename, SkipList *sl);

void process_cue_file(char *dir, const char *filename, SkipList *sl);

void process_m3u_file(char *dir, const char *filename, SkipList *sl);
