#pragma once

#include <stddef.h>
#include <common/base/options.h>

#define FOLDER_CUSTOM_PATH INFO_LAY_PATH "/folder"

enum { folder_changed_layout = 1, folder_changed_colour = 2 };

int folder_custom_path(const char *dir, char *out, size_t size);

int folder_custom_load(const char *dir);

int folder_custom_save(const char *dir, int own_layout);

const char *folder_custom_scheme(void);

void folder_custom_set_scheme(const char *scheme);

const char *folder_custom_colour(int role);

int folder_custom_count(void);

int folder_custom_reset_all(void);
