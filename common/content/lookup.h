#pragma once
#include <stddef.h>

typedef struct {
    const char *name;
    const char *value;
} lookup_internal_name;

#define LOOKUP_TABLE_COUNT 36

const char *lookup(const char *name);
void lookup_multi_at(
    size_t index, const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata
);

void r_lookup_multi_at(
    size_t index, const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata
);

void lookup_dump_at(size_t index, void (*emit)(const char *name, const char *value, void *udata), void *udata);
