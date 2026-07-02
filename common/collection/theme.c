#include <stdlib.h>
#include <string.h>
#include "theme.h"
#include "../strutil.h"

theme_item *add_theme_item(
    theme_item **theme_items, size_t *count, const char *name, const char *url, const int grid_enabled,
    const int hdmi_enabled, const int language_enabled, const int resolution640_x480, const int resolution720_x480,
    const int resolution720_x720, const int resolution1024_x768, const int resolution1280_x720,
    const int resolution1920_x1080
) {

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
    (*theme_items)[*count].resolution640_x480 = resolution640_x480;
    (*theme_items)[*count].resolution720_x480 = resolution720_x480;
    (*theme_items)[*count].resolution720_x720 = resolution720_x720;
    (*theme_items)[*count].resolution1024_x768 = resolution1024_x768;
    (*theme_items)[*count].resolution1280_x720 = resolution1280_x720;
    (*theme_items)[*count].resolution1920_x1080 = resolution1920_x1080;

    (*count)++;

    return &(*theme_items)[*count - 1];
}

int theme_item_compare(const void *a, const void *b) {
    const theme_item *item_a = (theme_item *) a;
    const theme_item *item_b = (theme_item *) b;

    // Case-insensitive natural sort on name, allocation-free
    return str_compare(&item_a->name, &item_b->name);
}

int theme_item_exists(const theme_item *theme_items, const size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(theme_items[i].name, name) == 0) return 1;
    }
    return 0;
}

void sort_theme_items(theme_item *theme_items, const size_t count) {
    qsort(theme_items, count, sizeof(theme_item), theme_item_compare);
}

theme_item get_theme_item_by_index(const theme_item *items, const size_t index) {
    return items[index];
}

void free_theme_items(theme_item **theme_items, size_t *count) {
    for (size_t i = 0; i < *count; i++) {
        free((*theme_items)[i].name);
        free((*theme_items)[i].url);
    }
    free(*theme_items);  // Free the array itself
    *theme_items = NULL; // Set the pointer to NULL
    *count = 0;          // Set the count to 0
}
