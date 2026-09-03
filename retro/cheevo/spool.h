#pragma once

#include <stddef.h>

int cheevo_spool_record(const char *post, char *name, size_t name_size);

void cheevo_spool_clear(const char *name);

int cheevo_spool_next(
    const char *username, const char *token, char *name, size_t name_size, char **post, size_t *post_size
);

unsigned cheevo_spool_count(void);
