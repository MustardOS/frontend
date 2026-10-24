#pragma once

#include <stddef.h>

void invalidate_catalogue_cache(void);

int is_supported_theme_catalogue(const char *catalogue_name, const char *image_type);

int load_image_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *program_default,
    const char *mux_dim, const char *image_type, char *image_path, size_t path_size
);

int load_media_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *program_default,
    const char *mux_dim, const char *image_type, char *image_path, size_t path_size
);

int load_splash_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *mux_dim, char *image_path,
    size_t path_size
);

int catalogue_media_is_animated(const char *path);

int load_png_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *program_default,
    const char *mux_dim, const char *image_type, char *image_path, size_t path_size
);

int load_manual_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, char *manual_path, size_t path_size
);
