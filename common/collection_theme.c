#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "collection_theme.h"
#include "common.h"

theme_item *add_theme_item(theme_item **theme_items, size_t *count, const char *name, const char *url,
                           bool grid_enabled, bool hdmi_enabled, bool language_enabled, bool resolution640x480,
                           bool resolution720x480, bool resolution720x720, bool resolution1024x768,
                           bool resolution1280x720) {

    if (*theme_items == NULL) {
        *theme_items = malloc(sizeof(theme_item));
    } else {
        *theme_items = realloc(*theme_items, (*count + 1) * sizeof(theme_item));
    }

    (*theme_items)[*count].name = strdup(name);
    (*theme_items)[*count].url = strdup(url);
    (*theme_items)[*count].grid_enabled = grid_enabled;
    (*theme_items)[*count].hdmi_enabled = hdmi_enabled;
    (*theme_items)[*count].language_enabled = language_enabled;
    (*theme_items)[*count].resolution640x480 = resolution640x480;
    (*theme_items)[*count].resolution720x480 = resolution720x480;
    (*theme_items)[*count].resolution720x720 = resolution720x720;
    (*theme_items)[*count].resolution1024x768 = resolution1024x768;
    (*theme_items)[*count].resolution1280x720 = resolution1280x720;

    (*count)++;

    return &(*theme_items)[*count - 1];
}

int theme_item_compare(const void *a, const void *b) {
    theme_item *itemA = (theme_item *) a;
    theme_item *itemB = (theme_item *) b;

    // Use strverscmp for natural sorting on sort_name
    return strverscmp(str_tolower(itemA->name), str_tolower(itemB->name));
}

int theme_item_exists(theme_item *theme_items, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(theme_items[i].name, name) == 0) return 1;
    }
    return 0;
}

void sort_theme_items(theme_item *theme_items, size_t count) {
    qsort(theme_items, count, sizeof(theme_item), theme_item_compare);
}

theme_item get_theme_item_by_index(theme_item *theme_items, size_t index) {
    return theme_items[index];
}

void free_theme_items(theme_item **theme_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free((*theme_items)[i].name);
        free((*theme_items)[i].url);
    }
    free(*theme_items); // Free the array itself
    *theme_items = NULL; // Set the pointer to NULL
    *count = 0; // Set the count to 0
}
