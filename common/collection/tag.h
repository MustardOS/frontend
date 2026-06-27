#pragma once

typedef struct {
    char *name;
    char *glyph;
    int sort_bucket;
} tag_item;

void load_tag_items(tag_item **tag_items, size_t *count);

int get_tag_sort_bucket(const tag_item *tag_items, size_t count, const char *glyph);

void sort_tag_items(tag_item *tag_items, size_t count);

void free_tag_items(tag_item **tag_items, size_t *count);

void print_tag_items(const tag_item *tag_items, size_t count);
