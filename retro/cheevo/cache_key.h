#pragma once

#include <stddef.h>

int cheevo_cache_name_valid(const char *name);
int cheevo_cache_request_name(const char *post, char *name, size_t name_size);
