#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../init.h"
#include "../log.h"
#include "../theme.h"
#include "../config.h"
#include "../fileio.h"
#include "glyph.h"

int resolve_glyph_size(const int16_t runtime_size, const int16_t section_size, const int auto_px) {
    const int size = runtime_size == -2 ? section_size : runtime_size;

    if (size < 0) return -1; // native
    if (size == 0) return auto_px;

    return size;
}

void append_glyph_size_hint(char *embed, const size_t embed_size, const int size) {
    if (size == 0 || !embed) return;

    const char *dot = strrchr(embed, '.');
    if (!dot || strncasecmp(dot, ".svg", 4) != 0) return;

    const size_t n = strlen(embed);
    snprintf(embed + n, embed_size - n, "?%dx%d", size, size);
}

void resolve_grid_glyph_hint(const struct theme_config *theme, int *hint_w, int *hint_h, int *px) {
    int16_t grid_glyph = config.settings.themeopt.glyph_size_grid;
    if (grid_glyph == -2) grid_glyph = theme->glyph.grid;

    if (grid_glyph < 0) {
        *hint_w = *hint_h = -1;
    } else if (grid_glyph == 0) {
        *hint_w = theme->grid.cell.width * 3 / 4;
        *hint_h = theme->grid.cell.height * 3 / 4;
    } else {
        *hint_w = *hint_h = grid_glyph;
    }

    *px = grid_glyph > 0 ? grid_glyph : 0;
}

void build_embed_path(char *embed, const size_t embed_size, const char *path, const int hint_w, const int hint_h) {
    const int written = snprintf(embed, embed_size, "M:%s", path);
    if (written < 0 || (size_t) written >= embed_size) {
        embed[0] = '\0';
        return;
    }

    const size_t len = strlen(path);
    if (len <= 4 || strcmp(path + len - 4, ".svg") != 0) return;

    snprintf(embed + written, embed_size - (size_t) written, "?%dx%d", hint_w, hint_h);
}

void apply_glyph_scale(lv_obj_t *img, const char *embed, const int box_w, const int box_h) {
    lv_img_set_zoom(img, LV_IMG_ZOOM_NONE);
    if (!embed || box_w <= 0 || box_h <= 0) return;

    const char *q = strchr(embed, '?');
    const size_t len = q ? (size_t) (q - embed) : strlen(embed);
    if (len >= 4 && strncasecmp(embed + len - 4, ".svg", 4) == 0) return;

    lv_img_header_t header;
    if (lv_img_decoder_get_info(embed, &header) != LV_RES_OK) return;
    if (header.w <= 0 || header.h <= 0) return;

    const long zoom_w = (long) box_w * LV_IMG_ZOOM_NONE / header.w;
    const long zoom_h = (long) box_h * LV_IMG_ZOOM_NONE / header.h;
    long zoom = zoom_w < zoom_h ? zoom_w : zoom_h;

    if (zoom < 1) zoom = 1;
    if (zoom > 0xFFFF) zoom = 0xFFFF;

    lv_img_set_zoom(img, (uint16_t) zoom);
}

int glyph_explicit_px(const int16_t runtime_size, const int16_t section_size) {
    const int size = runtime_size == -2 ? section_size : runtime_size;

    return size > 0 ? size : 0;
}

void set_list_glyph_image(lv_obj_t *img, const char *embed) {
    lv_img_set_src(img, embed);
    const int px = glyph_explicit_px(config.settings.themeopt.glyph_size_list, theme.glyph.list);
    apply_glyph_scale(img, embed, px, px);
}

int get_glyph_path(
    const char *mux_module, const char *glyph_name, char *glyph_image_embed, size_t glyph_image_embed_size
) {
    char glyph_image_path[MAX_BUFFER_SIZE];

#define TRY_GLYPH_PATH(fmt, ...)                                                                                       \
    do {                                                                                                               \
        snprintf(glyph_image_path, sizeof(glyph_image_path), fmt, ##__VA_ARGS__);                                      \
        LOG_DEBUG(mux_module, "Glyph path check: %s", glyph_image_path);                                               \
        if (file_exist(glyph_image_path)) {                                                                            \
            LOG_DEBUG(mux_module, "Glyph found at: %s", glyph_image_path);                                             \
            snprintf(glyph_image_embed, glyph_image_embed_size, "M:%s", glyph_image_path);                             \
            append_glyph_size_hint(                                                                                    \
                glyph_image_embed, glyph_image_embed_size,                                                             \
                resolve_glyph_size(                                                                                    \
                    config.settings.themeopt.glyph_size_list, theme.glyph.list, theme.mux.item.height * 3 / 4          \
                )                                                                                                      \
            );                                                                                                         \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

    TRY_GLYPH_PATH("%s/%sglyph/%s/%s.svg", theme_base, mux_dim, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/glyph/%s/%s.svg", theme_base, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/%sglyph/%s/%s.png", theme_base, mux_dim, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/glyph/%s/%s.png", theme_base, mux_module, glyph_name);

    TRY_GLYPH_PATH("%s/%sglyph/%s/%s.svg", INTERNAL_THEME, mux_dim, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/glyph/%s/%s.svg", INTERNAL_THEME, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/%sglyph/%s/%s.png", INTERNAL_THEME, mux_dim, mux_module, glyph_name);
    TRY_GLYPH_PATH("%s/glyph/%s/%s.png", INTERNAL_THEME, mux_module, glyph_name);

#undef TRY_GLYPH_PATH

    LOG_DEBUG(mux_module, "Glyph not found: %s/%s", mux_module, glyph_name);
    return 0;
}

void apply_app_glyph(const char *app_folder, const char *glyph_name, lv_obj_t *ui_lbl_item_glyph) {
    char glyph_image_path[MAX_BUFFER_SIZE];

#define TRY_APP_GLYPH(fmt, ...)                                                                                        \
    do {                                                                                                               \
        snprintf(glyph_image_path, sizeof(glyph_image_path), fmt, ##__VA_ARGS__);                                      \
        LOG_DEBUG(mux_module, "Application glyph path check: %s", glyph_image_path);                                   \
        if (file_exist(glyph_image_path)) {                                                                            \
            LOG_DEBUG(mux_module, "Application glyph found at: %s", glyph_image_path);                                 \
            char glyph_image_embed[MAX_BUFFER_SIZE];                                                                   \
            snprintf(glyph_image_embed, sizeof(glyph_image_embed), "M:%s", glyph_image_path);                          \
            append_glyph_size_hint(                                                                                    \
                glyph_image_embed, sizeof(glyph_image_embed),                                                          \
                resolve_glyph_size(                                                                                    \
                    config.settings.themeopt.glyph_size_list, theme.glyph.list, theme.mux.item.height * 3 / 4          \
                )                                                                                                      \
            );                                                                                                         \
            set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);                                                \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

    TRY_APP_GLYPH("%s/glyph/%s%s.svg", app_folder, mux_dim, glyph_name);
    TRY_APP_GLYPH("%s/glyph/%s.svg", app_folder, glyph_name);

    TRY_APP_GLYPH("%s/glyph/%s%s.png", app_folder, mux_dim, glyph_name);
    TRY_APP_GLYPH("%s/glyph/%s.png", app_folder, glyph_name);

#undef TRY_APP_GLYPH

    LOG_DEBUG(mux_module, "Application glyph not found: %s/%s", app_folder, glyph_name);
}

void get_app_grid_glyph(
    const char *app_folder, const char *glyph_name, const char *fallback_name, char *glyph_image_path,
    size_t glyph_image_path_size
) {
    (void) fallback_name;

    if (!glyph_image_path || glyph_image_path_size == 0) return;
    if (!app_folder || !app_folder[0] || !glyph_name || !glyph_name[0]) return;

    char dim_clean[MAX_BUFFER_SIZE];
    dim_clean[0] = '\0';

    if (mux_dim[0]) {
        snprintf(dim_clean, sizeof(dim_clean), "%s", mux_dim);

        size_t len = strlen(dim_clean);
        while (len > 0 && dim_clean[len - 1] == '/') {
            dim_clean[--len] = '\0';
        }
    }

    char image_path[MAX_BUFFER_SIZE];

#define TRY_APP_GRID(fmt, ...)                                                                                         \
    do {                                                                                                               \
        int _written = snprintf(image_path, sizeof(image_path), fmt, ##__VA_ARGS__);                                   \
        if (_written < 0 || (size_t) _written >= sizeof(image_path)) break;                                            \
        LOG_DEBUG(mux_module, "Application grid image check: %s", image_path);                                         \
        if (file_exist(image_path)) {                                                                                  \
            LOG_DEBUG(mux_module, "Application grid image found: %s", image_path);                                     \
            snprintf(glyph_image_path, glyph_image_path_size, "%s", image_path);                                       \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

    if (dim_clean[0]) TRY_APP_GRID("%s/grid/%s/%s.svg", app_folder, dim_clean, glyph_name);
    TRY_APP_GRID("%s/grid/%s.svg", app_folder, glyph_name);

    if (dim_clean[0]) TRY_APP_GRID("%s/grid/%s/%s.png", app_folder, dim_clean, glyph_name);
    TRY_APP_GRID("%s/grid/%s.png", app_folder, glyph_name);

#undef TRY_APP_GRID
}
