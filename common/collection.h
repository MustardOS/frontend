#pragma once

#include <stddef.h>

typedef enum {
    MENU,
    FOLDER,
    ITEM
} content_type;

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
    char group_tag[64];
} content_item;

int is_in_list(char **list, int count, const char *sys_dir, const char *name);

content_item *add_item(content_item **content_items, size_t *count, const char *name, const char *sort_name,
                       const char *extra_data, content_type content_type);

void remove_item(content_item **content_items, size_t *count, size_t index);

int bucket_item_compare(const void *a, const void *b);

int item_exists(content_item *content_items, size_t count, const char *name);

void sort_items(content_item *content_items, size_t count);

void sort_items_time(content_item *content_items, size_t count);

content_item get_item_by_index(content_item *items, size_t index);

int get_item_index_by_name(content_item *content_items, size_t count, const char *name, content_type type);

int get_item_index_by_extra_data(content_item *content_items, size_t count, const char *extra_data);

void free_items(content_item **content_items, size_t *count);

void free_item_list(char ***list, int *count);

void print_items(content_item *content_items, size_t count);
