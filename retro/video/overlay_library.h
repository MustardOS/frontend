#pragma once

#include <stddef.h>
#include <common/base/options.h>
#include "../core/paths.h"

#define OVERLAY_IMAGE_ROOT RETRO_SHARE_PATH "overlay/"
#define OVERLAY_IMAGE_SYS  OPT_SHARE_PATH "overlay/image/"
#define OVERLAY_IMAGE_MAX  (8 * 1024 * 1024)

int overlay_library_dir(char *out, size_t out_size);

void overlay_library_refresh(void);

int overlay_library_count(void);

const char *overlay_library_label(int index);

const char *overlay_library_key(int index);

int overlay_library_index(const char *key);

int overlay_library_path(int index, char *out, size_t out_size);

int overlay_library_file_valid(const char *path);

int overlay_library_is_user(int index);

int overlay_library_delete(int index);
