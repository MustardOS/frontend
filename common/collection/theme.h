#pragma once

#include <stddef.h>

typedef struct {
    char *name;
    char *url;
    int grid_enabled;
    int hdmi_enabled;
    int language_enabled;
    int resolution640x480;
    int resolution720x480;
    int resolution720x720;
    int resolution1024x768;
    int resolution1280x720;
    int resolution1920x1080;
} theme_item;

theme_item *add_theme_item(theme_item **theme_items, size_t *count, const char *name, const char *url,
                           int grid_enabled, int hdmi_enabled, int language_enabled, int resolution640x480,
                           int resolution720x480, int resolution720x720, int resolution1024x768,
                           int resolution1280x720, int resolution1920x1080);

int theme_item_exists(theme_item *theme_items, size_t count, const char *name);

void sort_theme_items(theme_item *theme_items, size_t count);

theme_item get_theme_item_by_index(theme_item *items, size_t index);

void free_theme_items(theme_item **theme_items, size_t *count);
