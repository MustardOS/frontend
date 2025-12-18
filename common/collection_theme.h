#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *name;
    char *url;
    bool grid_enabled;
    bool hdmi_enabled;
    bool language_enabled;
    bool resolution640x480;
    bool resolution720x480;
    bool resolution720x720;
    bool resolution1024x768;
    bool resolution1280x720;
} theme_item;

theme_item *add_theme_item(theme_item **theme_items, size_t *count, const char *name, const char *url,
                           bool grid_enabled, bool hdmi_enabled, bool language_enabled, bool resolution640x480,
                           bool resolution720x480, bool resolution720x720, bool resolution1024x768,
                           bool resolution1280x720);

int theme_item_exists(theme_item *theme_items, size_t count, const char *name);

void sort_theme_items(theme_item *theme_items, size_t count);

theme_item get_theme_item_by_index(theme_item *items, size_t index);

void free_theme_items(theme_item **theme_items, size_t *count);
