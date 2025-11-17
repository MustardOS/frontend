#pragma once

#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} SkipList;

void init_skiplist(SkipList *sl);

void free_skiplist(SkipList *sl);

void add_to_skiplist(SkipList *sl, const char *dir, const char *name);

bool in_skiplist(const SkipList *sl, const char *name);

bool ends_with(char *str, const char *suffix);

void process_gdi_file(char *dir, const char *filename, SkipList *sl);

void process_cue_file(char *dir, const char *filename, SkipList *sl);

void process_m3u_file(char *dir, const char *filename, SkipList *sl);

#endif
