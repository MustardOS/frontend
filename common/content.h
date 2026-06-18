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
    CONTENT_NAME = 1,
    CONTENT_CORE,
    CONTENT_SYSTEM,
    CONTENT_CATALOGUE,
    CONTENT_LOOKUP,
    CONTENT_ASSIGN,
    CONTENT_MOUNT,
    CONTENT_DIR,
    CONTENT_FULL
};

enum global_type {
    GLOBAL_CORE = 1,
    GLOBAL_SYSTEM,
    GLOBAL_CATALOGUE,
    GLOBAL_LOOKUP,
    GLOBAL_ASSIGN
};

enum cache_type {
    CACHE_CORE_PATH = 1,
    CACHE_CORE_DIR,
    CACHE_CORE_NAME
};

extern char **history_items;
extern int history_item_count;
extern char **collection_items;
extern int collection_item_count;

char *get_content_line(char *dir, char *name, char *ext, size_t line);

char *get_application_line(char *dir, char *ext, size_t line);

void populate_history_items(void);

void populate_collection_items(void);

char *get_content_explorer_glyph_name(char *file_path);

uint32_t fnv1a_hash_str(const char *str);

uint32_t fnv1a_hash_file(FILE *fp);

int load_content(int add_collection, char *file_path);

char *load_content_core(int force, int run_quit, char *file_path);

char *build_core(char core_path[MAX_BUFFER_SIZE], int line_core, int line_system,
                 int line_catalogue, int line_lookup, int line_launch);

void rewrite_launch_file(const char *file, const char *new_path);

void migrate_history_entry(const char *old_file, const char *new_path, const char *content_name);

void check_collection(const char *col_file);

void add_to_collection(char *filename, const char *pointer, char *sys_dir);
