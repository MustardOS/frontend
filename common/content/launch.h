#pragma once

#include <stddef.h>

int launch_marker_path(const char *dir_path, char *out, size_t out_size);

int folder_marked_launch(const char *dir_path, char *out_path, size_t out_size);

int launch_marker_matches(const char *file_path);

int launch_marker_set(const char *file_path);

int launch_marker_clear(const char *dir_path);

int content_launch_folder_name(const char *file_path, char *out, size_t out_size);
