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
    char *use_module;
} content_item;

content_item *add_item(content_item **content_items, size_t *count, const char *name, const char *sort_name,
                       const char *extra_data, content_type content_type);

int item_exists(content_item *content_items, size_t count, const char *name);

void sort_items(content_item *content_items, size_t count);

void sort_items_time(content_item *content_items, size_t count);

content_item get_item_by_index(content_item *items, size_t index);

int get_folder_item_index_by_name(content_item *content_items, size_t count, const char *name);

void free_items(content_item **content_items, size_t *count);

void print_items(content_item *content_items, size_t count);
