#pragma once

#include <stddef.h>
#include <stdint.h>

void union_get_root_mount(char *out, size_t out_size);

void union_get_relative_path(const char *path, char *out, size_t out_size);

void union_get_mount_path(const char *path, char *out, size_t out_size);

int union_is_root(const char *path);

int union_get_directory_item_count(const char *base_dir, const char *name, int count_type);

int union_collect(const char *base_dir, char ***dir_names, char ***dir_paths, int *dir_count,
                  char ***file_names, char ***file_paths, int *file_count, int **dir_item_counts);

void union_get_roms_root(char *out, size_t out_size);

char *union_get_title_root(char *path, char *out, size_t out_size);

int union_resolve_to_real(const char *union_path, char *out, size_t out_size);

int union_rewrite_file_paths(const char *file);
