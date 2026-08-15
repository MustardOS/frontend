#pragma once
#include <stddef.h>

typedef struct {
    char *name;
    char *glyph;
    int sort_bucket;
} tag_item;

void load_tag_items(tag_item **tag_items, size_t *count);

int get_tag_sort_bucket(const tag_item *tag_items, size_t count, const char *glyph);

void sort_tag_items(tag_item *tag_items, size_t count);

void free_tag_items(tag_item **tag_items, size_t *count);

char *read_content_tag(const char *sub_path, const char *content_path);

const char *tag_filter_path(void);

int tag_filter_get(char *output, size_t size);

void tag_filter_apply(void);
