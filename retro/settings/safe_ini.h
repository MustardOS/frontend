#pragma once

#include <stddef.h>
#include <mini/mini.h>

enum safe_ini_value_status { safe_ini_value_invalid = -1, safe_ini_value_missing = 0, safe_ini_value_valid = 1 };

mini_t *safe_ini_load(const char *path, size_t maximum_size);

enum safe_ini_value_status safe_ini_get_int(mini_t *ini, const char *group, const char *key, long long *value);

int safe_ini_get_int_range(
    mini_t *ini, const char *group, const char *key, long long minimum, long long maximum, int fallback, int *value
);
