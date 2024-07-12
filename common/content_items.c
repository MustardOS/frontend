#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "content_item.h"
#include "content_items.h"

void initialise_array(struct content_items* item) {
    item->array = (struct ContentItem **)malloc(sizeof(struct ContentItem*));
    item->size = 0;
    item->capacity = 1;
}

void push_item(struct content_items* item, struct ContentItem* content) {
    if (item->size == item->capacity) {
        item->capacity *= 2;
        item->array = (struct ContentItem**)realloc(item->array, item->capacity * sizeof(struct ContentItem*));
    }

    item->array[item->size] = content;
    item->size++;
}

struct ContentItem* get_item_at_index(struct content_items* item, int index) {
    if (index < item->size) {
        return item->array[index];
    } else {
        return NULL;
    }
}

void free_array(struct content_items* item) {
    for (size_t i = 0; i < item->size; i++) {
        // Assuming you need to free any dynamically allocated memory within ContentItem
        // free(item->array[i]->someMember);
        free(item->array[i]);
    }
    free(item->array);
    item->size = 0;
    item->capacity = 0;
}

void print_array(struct content_items* item, int split) {
    printf("[\n");
    for (int i = 0; i < item->size; i++) {
        if (i > 0 && (i + 1) % split == 0) {
            printf("");
        }
        printf("ContentItem[%d]", i); // Modify this to print relevant info from ContentItem
        if (i < item->size - 1) {
            printf(", ");
        }
        if ((i + 1) % split == 0) {
            printf("\n");
        }
    }
    printf("\n]\n");
}