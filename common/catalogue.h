#pragma once

#include <stddef.h>

void invalidate_catalogue_cache(void);

void load_splash_image_fallback(const char *mux_dim, char *image, size_t image_size);

int is_supported_theme_catalogue(const char *catalogue_name, const char *image_type);

int load_image_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *program_default,
    const char *mux_dim, const char *image_type, char *image_path, size_t path_size
);

int load_video_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *mux_dim, char *video_path,
    size_t path_size
);
