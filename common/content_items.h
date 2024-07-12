#pragma once

#include <stddef.h>
#include "content_item.h"

struct content_items {
    struct ContentItem **array;
    int size;
    int capacity;
};

void initialise_array(struct content_items *item);

void push_item(struct content_items *item, struct ContentItem* value);

struct ContentItem* get_item_at_index(struct content_items *item, int index);

void free_array(struct content_items *item);

void print_array(struct content_items *item, int split);
