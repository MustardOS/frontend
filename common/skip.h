#pragma once

typedef struct {
    char **items;
    size_t count;
    size_t capacity;

    char **buckets;
    size_t bucket_cap;
} skip_list;

void init_skiplist(skip_list *sl);

void free_skiplist(skip_list *sl);

void add_to_skiplist(skip_list *sl, const char *dir, const char *name);

int in_skiplist(const skip_list *sl, const char *name);

int ends_with(char *str, const char *suffix);

void process_gdi_file(char *dir, const char *filename, skip_list *sl);

void process_cue_file(char *dir, const char *filename, skip_list *sl);

void process_m3u_file(char *dir, const char *filename, skip_list *sl);
