#pragma once

#include <stddef.h>

enum {
    theme_include_colours = 1 << 0,
    theme_include_layouts = 1 << 1,
    theme_include_menu = 1 << 2,
    theme_include_font = 1 << 3,
};

enum { theme_create_ok, theme_create_name, theme_create_exists, theme_create_failed, theme_create_unsafe };

int theme_create(const char *name, unsigned include, char *folder, size_t folder_size);

int theme_lab_own(const char *folder);

int theme_delete(const char *folder);

int theme_remix(const char *source, char *folder, size_t folder_size);

int theme_rename(const char *folder, const char *name, char *out, size_t out_size);
