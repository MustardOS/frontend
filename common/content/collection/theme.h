#pragma once
#include <stddef.h>

typedef struct {
    char *name;
    char *url;
    int grid_enabled;
    int hdmi_enabled;
    int language_enabled;
    int resolution640_x480;
    int resolution720_x480;
    int resolution720_x720;
    int resolution1024_x768;
    int resolution1280_x720;
    int resolution1920_x1080;
} theme_item;

theme_item *add_theme_item(
    theme_item **theme_items, size_t *count, const char *name, const char *url, int grid_enabled, int hdmi_enabled,
    int language_enabled, int resolution640_x480, int resolution720_x480, int resolution720_x720,
    int resolution1024_x768, int resolution1280_x720, int resolution1920_x1080
);

void sort_theme_items(theme_item *theme_items, size_t count);

void free_theme_items(theme_item **theme_items, size_t *count);
