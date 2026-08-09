#pragma once

#include <stddef.h>
#include "cache_key.h"

int cheevo_cache_load(const char *name, char **body, size_t *body_size);
void cheevo_cache_store(const char *name, const char *body, size_t body_size);
