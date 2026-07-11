#pragma once

#include <stddef.h>

int patch_exists(const char *base_path);

int patch_apply(const char *base_path, void **data, size_t *size, char *patch_list, size_t patch_list_size);
