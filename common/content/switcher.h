#pragma once

#include <stddef.h>

typedef enum {
    content_switch_source_history,
    content_switch_source_collection,
} content_switch_source;

typedef enum {
    content_switch_native_none,
    content_switch_native_pickles,
    content_switch_native_wasabi,
} content_switch_native;

typedef struct {
    char *path;
    char *title;
    content_switch_source source;
    content_switch_native native;
    long modified;
} content_switch_entry;

typedef struct {
    content_switch_entry *entries;
    size_t count;
    size_t selected;
} content_switch_list;

int content_switch_load(content_switch_list *list, const char *current_path);

void content_switch_free(content_switch_list *list);

int content_switch_write_request(const char *path, content_switch_native native);
