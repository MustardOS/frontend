#pragma once

#include <stddef.h>

typedef enum { content_type_menu, content_type_folder, content_type_item } content_type;

typedef struct {
    size_t play_time;
    size_t times_played;
    long last_played;
    long added;
    long file_size;
    size_t title_length;
    size_t vowels;
    size_t words;
    size_t capitals;
    int sequel;
    unsigned int shuffle;
} order_key;

typedef struct {
    char *name;
    char *display_name;
    char *sort_name;
    content_type content_type;
    char *extra_data;
    char *help;
    char *glyph_icon;
    char *grid_image;
    char *grid_image_focused;
    char *use_module;
    int sort_bucket;
    int folder_item_count;
    char group_tag[64];
    order_key order;
} content_item;

content_item *add_item(
    content_item **content_items, size_t *count, const char *name, const char *sort_name, const char *extra_data,
    content_type content_type
);

void remove_item(content_item **content_items, size_t *count, size_t index);

int bucket_item_compare(const void *a, const void *b);

int item_exists(const content_item *content_items, size_t count, const char *name);

void sort_items(content_item *content_items, size_t count);

void sort_items_time(content_item *content_items, size_t count);

content_item get_item_by_index(const content_item *items, size_t index);

int get_item_index_by_name(const content_item *content_items, size_t count, const char *name, content_type type);

int get_item_index_by_extra_data(const content_item *content_items, size_t count, const char *extra_data);

void free_items(content_item **content_items, size_t *count);

void free_item_list(char ***list, int *count);

void print_items(const content_item *content_items, size_t count);
