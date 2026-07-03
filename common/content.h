#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "options.h"

/* The following enums start at 1 because the values are used to
 * reference specific line numbers in associated core content files.
 *
 * Index 0 is unused, and line 'n' corresponds directly to value 'n'.
 */
enum content_field {
    content_name = 1,
    content_core,
    content_system,
    content_catalogue,
    content_lookup,
    content_assign,
    content_mount,
    content_dir,
    content_full
};

enum global_type { global_core = 1, global_system, global_catalogue, global_lookup, global_assign };

enum cache_type { cache_core_path = 1, cache_core_dir, cache_core_name };

extern char **history_items;
extern int history_item_count;
extern char **collection_items;
extern int collection_item_count;

char *get_content_line(char *dir, const char *name, char *ext, size_t line);

char *get_application_line(char *dir, char *ext, size_t line);

void populate_history_items(void);

void populate_collection_items(void);

int history_set_contains(const char *path);

int collection_set_contains(const char *path);

void mark_history_dirty(void);

void mark_collection_dirty(void);

char *get_content_explorer_glyph_name(const char *file_path);

uint32_t fnv_hash_str(const char *str);

uint32_t fnv_hash_file(FILE *fp);

int load_content(int add_collection, char *file_path);

char *load_content_core(int force, int run_quit, char *file_path);

char *build_core(
    char core_path[MAX_BUFFER_SIZE], int line_core, int line_system, int line_catalogue, int line_lookup,
    int line_launch
);

void rewrite_launch_file(const char *file, const char *new_path);

void migrate_history_entry(const char *old_file, const char *new_path, const char *content_name);

void check_collection(const char *col_file);

void add_to_collection(char *filename, const char *pointer, char *sys_dir);
