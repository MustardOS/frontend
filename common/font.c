#include <stdlib.h>
#include <stdio.h>
#include "font.h"
#include "common.h"
#include "theme.h"
#include "log.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "input/list_nav.h"

// Max TTF file size accepted (64 MB). Protects against accidentally pointing at a
// giant file and exhausting RAM before we've had a chance to log anything useful.
#define TTF_MAX_FILE_BYTES (64 * 1024 * 1024)

// Glyph bitmap cache handed to TinyTTF. Larger = fewer re-rasterisations of the
// same glyph, which matters most for CJK where each character is unique.
// At 30px each glyph slot is ~900 bytes... so 512 KB gives ~580 slots vs ~290 at
// 256kb which enough headroom to keep a full Latin symbol set during scrolling.
#define TTF_GLYPH_CACHE_BYTES (512 * 1024)

int font_cache_count = 0;
char last_font_key[256] = "";

typedef struct {
    char path[MAX_BUFFER_SIZE];
    int size;

    lv_font_t *font;

    void *data;
    int is_ttf;
} font_cache_t;

static font_cache_t font_cache[FONT_CACHE_MAX];

int get_font_size(void) {
    switch (device.MUX.WIDTH) {
        case 1024:
            return 32;
        case 1280:
            return 30;
        case 1920:
            return 36;
        default:
            return 20;
    }
}

static int get_ttf_size(void) {
    return (theme.FONT.FONT_LIST_SIZE > 0) ? (int) theme.FONT.FONT_LIST_SIZE : get_font_size();
}

static int get_section_ttf_size(const char *section) {
    if (strcmp(section, FONT_HEADER_DIR) == 0 && theme.FONT.FONT_HEADER_SIZE > 0) return (int) theme.FONT.FONT_HEADER_SIZE;
    if (strcmp(section, FONT_FOOTER_DIR) == 0 && theme.FONT.FONT_FOOTER_SIZE > 0) return (int) theme.FONT.FONT_FOOTER_SIZE;
    if (strcmp(section, FONT_PANEL_DIR) == 0 && grid_mode_enabled && theme.FONT.FONT_PANEL_SIZE > 0) return (int) theme.FONT.FONT_PANEL_SIZE;
    return get_font_size();
}

static int get_custom_section_size(const char *section) {
    if (strcmp(section, FONT_HEADER_DIR) == 0 && config.SETTINGS.FONT.HEADER_SIZE > 0) return config.SETTINGS.FONT.HEADER_SIZE;
    if (strcmp(section, FONT_FOOTER_DIR) == 0 && config.SETTINGS.FONT.FOOTER_SIZE > 0) return config.SETTINGS.FONT.FOOTER_SIZE;
    if (strcmp(section, FONT_PANEL_DIR) == 0 && grid_mode_enabled && config.SETTINGS.FONT.PANEL_SIZE > 0) return config.SETTINGS.FONT.PANEL_SIZE;
    return (config.SETTINGS.FONT.LIST_SIZE > 0) ? config.SETTINGS.FONT.LIST_SIZE : get_font_size();
}

static lv_font_t *load_font_cached_ttf(const char *path, int size, bool set_fallback);

static lv_font_t *create_language_font(int size) {
    const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;
    const char *name = config.SETTINGS.FONT.NAME;

    if (config.SETTINGS.ADVANCED.FONT == 0 && curr_lang[0] && name[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s/%s.ttf", curr_lang, name);
        lv_font_t * font = load_font_cached_ttf(path, size, false);
        if (font) return font;
    }

    return NULL;
}

lv_font_t *get_language_font(void) {
    return create_language_font(get_font_size());
}

void font_cache_clear(void) {
    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].is_ttf) {
            lv_tiny_ttf_destroy(font_cache[i].font);
        } else {
            lv_font_free(font_cache[i].font);
        }

        free(font_cache[i].data);
    }

    font_cache_count = 0;
    LOG_SUCCESS(mux_module, "Font cache has been cleared");
}

static inline void apply_font(lv_obj_t *element, lv_font_t *font) {
    if (!element || !font) return;
    lv_obj_set_style_text_font(element, font, MU_OBJ_MAIN_DEFAULT);
}

static lv_font_t *load_font_from_bin(const char *filepath) {
    LOG_WARN(mux_module, "BIN font support is deprecated; please use TTF: %s", filepath);

    char fs_path[MAX_BUFFER_SIZE];
    snprintf(fs_path, sizeof(fs_path), "M:%s", filepath);

    lv_font_t * font = lv_font_load(fs_path);
    if (!font) return NULL;

    return font;
}

static lv_font_t *load_font_from_ttf(const char *filepath, int size, void **out_data) {
    *out_data = NULL;

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        LOG_WARN(mux_module, "Cannot open TTF font: %s", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size <= 0 || file_size > TTF_MAX_FILE_BYTES) {
        LOG_WARN(mux_module, "TTF font %s has unexpected size (%ld bytes)", filepath, file_size);
        fclose(f);
        return NULL;
    }

    void *data = malloc((size_t) file_size);
    if (!data) {
        LOG_ERROR(mux_module, "Out of memory loading TTF font (%ld bytes): %s", file_size, filepath);
        fclose(f);
        return NULL;
    }

    if ((long) fread(data, 1, (size_t) file_size, f) != file_size) {
        LOG_WARN(mux_module, "Short read on TTF font: %s", filepath);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    lv_font_t * font = lv_tiny_ttf_create_data_ex(data, (size_t) file_size, (lv_coord_t) size, TTF_GLYPH_CACHE_BYTES);
    if (!font) {
        LOG_WARN(mux_module, "TinyTTF failed to parse: %s", filepath);
        free(data);
        return NULL;
    }

    LOG_INFO(mux_module, "TTF font loaded into memory (%ld KB): %s", file_size / 1024, filepath);

    *out_data = data;
    return font;
}

static lv_font_t *cache_lookup(const char *path, int size) {
    for (int i = 0; i < font_cache_count; i++) {
        if (font_cache[i].size == size && strcmp(font_cache[i].path, path) == 0) return font_cache[i].font;
    }

    return NULL;
}

static void cache_store(const char *path, int size, lv_font_t *font, void *data, int is_ttf) {
    if (font_cache_count >= FONT_CACHE_MAX) {
        if (is_ttf) {
            lv_tiny_ttf_destroy(font);
            free(data);
        } else {
            lv_font_free(font);
        }

        LOG_WARN(mux_module, "Font cache full; discarding: %s", path);
        return;
    }

    snprintf(font_cache[font_cache_count].path, MAX_BUFFER_SIZE, "%s", path);

    font_cache[font_cache_count].size = size;
    font_cache[font_cache_count].font = font;
    font_cache[font_cache_count].data = data;
    font_cache[font_cache_count].is_ttf = is_ttf;

    font_cache_count++;
}

static lv_font_t *load_font_cached_bin(const char *path) {
    lv_font_t * hit = cache_lookup(path, 0);
    if (hit) return hit;

    lv_font_t * font = load_font_from_bin(path);
    if (!font) return NULL;

    cache_store(path, 0, font, NULL, 0);
    font->fallback = get_language_font();

    return font;
}

static lv_font_t *load_font_cached_ttf(const char *path, int size, bool set_fallback) {
    lv_font_t * hit = cache_lookup(path, size);
    if (hit) return hit;

    void *data = NULL;

    lv_font_t * font = load_font_from_ttf(path, size, &data);
    if (!font) return NULL;

    cache_store(path, size, font, data, 1);

    if (set_fallback) font->fallback = get_language_font();

    return font;
}

static lv_font_t *try_font_at(const char *base, char *resolved, int size) {
    char path[MAX_BUFFER_SIZE];

    snprintf(path, sizeof(path), "%s.ttf", base);
    if (file_exist(path)) {
        lv_font_t * f = load_font_cached_ttf(path, size, true);
        if (f) {
            snprintf(resolved, 1024, "%s", path);
            return f;
        }
    }

    snprintf(path, sizeof(path), "%s.bin", base);
    if (file_exist(path)) {
        lv_font_t * f = load_font_cached_bin(path);
        if (f) {
            snprintf(resolved, 1024, "%s", path);
            return f;
        }
    }

    return NULL;
}

lv_font_t *load_font_pass_roller(void) {
    int size = (device.MUX.WIDTH >= 1280) ? 48 : 32;

    if (config.SETTINGS.ADVANCED.FONT == 2 && config.SETTINGS.FONT.NAME[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", config.SETTINGS.FONT.NAME);
        lv_font_t * f = load_font_cached_ttf(path, size, false);
        if (f) return f;
    }

    return create_language_font(size);
}

void load_font_text(lv_obj_t *screen) {
    int lang_size = (config.SETTINGS.ADVANCED.FONT == 0 && config.SETTINGS.FONT.LIST_SIZE > 0) ? config.SETTINGS.FONT.LIST_SIZE : get_font_size();
    lv_font_t * language_font = create_language_font(lang_size);

    if (config.SETTINGS.ADVANCED.FONT == 2 && config.SETTINGS.FONT.NAME[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", config.SETTINGS.FONT.NAME);

        int size = (config.SETTINGS.FONT.LIST_SIZE > 0) ? config.SETTINGS.FONT.LIST_SIZE : get_font_size();
        lv_font_t * font = load_font_cached_ttf(path, size, true);

        if (font) {
            LOG_INFO(mux_module, "Loading Custom Font: %s", path);
            apply_font(screen, font);
            return;
        }
    }

    if (config.SETTINGS.ADVANCED.FONT == 1) {
        const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;

        char *dims[2] = {mux_dim, ""};
        int size = get_ttf_size();

        char base[MAX_BUFFER_SIZE];
        char resolved[MAX_BUFFER_SIZE];

        lv_font_t * font = NULL;

        if (grid_mode_enabled) {
            for (int i = 0; i < 2 && !font; i++) {
                snprintf(base, sizeof(base), "%s/%sfont/%s/grid/%s", theme_base, dims[i], curr_lang, mux_module);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/%s/grid/default", theme_base, dims[i], curr_lang);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/grid/%s", theme_base, dims[i], mux_module);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/grid/default", theme_base, dims[i]);

                font = try_font_at(base, resolved, size);
            }
        }

        if (!font) {
            for (int i = 0; i < 2 && !font; i++) {
                snprintf(base, sizeof(base), "%s/%sfont/%s/%s", theme_base, dims[i], curr_lang, mux_module);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/%s/default", theme_base, dims[i], curr_lang);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/%s", theme_base, dims[i], mux_module);

                if ((font = try_font_at(base, resolved, size))) break;
                snprintf(base, sizeof(base), "%s/%sfont/default", theme_base, dims[i]);

                font = try_font_at(base, resolved, size);
            }
        }

        if (font) {
            LOG_INFO(mux_module, "Loading Theme Font: %s", resolved);
            apply_font(screen, font);
            return;
        }
    }

    LOG_INFO(mux_module, "Loading Default Language Font");
    apply_font(screen, language_font);
}

void load_font_section(const char *section, lv_obj_t *element) {
    if (config.SETTINGS.ADVANCED.FONT == 2 && config.SETTINGS.FONT.NAME[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", config.SETTINGS.FONT.NAME);

        int size = get_custom_section_size(section);
        lv_font_t * font = load_font_cached_ttf(path, size, true);

        if (font) {
            LOG_INFO(mux_module, "Loading Custom Section '%s' Font: %s", section, path);
            apply_font(element, font);
            return;
        }

        if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
            apply_font(element, create_language_font(size));
        } else {
            lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
        }

        return;
    }

    if (!config.SETTINGS.ADVANCED.FONT) {
        int size = get_custom_section_size(section);
        if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
            apply_font(element, create_language_font(size));
        } else {
            lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
        }

        return;
    }

    const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;

    char *dims[2] = {mux_dim, ""};
    int size = get_section_ttf_size(section);

    char base[MAX_BUFFER_SIZE];
    char resolved[MAX_BUFFER_SIZE];

    lv_font_t * font = NULL;

    if (grid_mode_enabled) {
        for (int i = 0; i < 2 && !font; i++) {
            snprintf(base, sizeof(base), "%s/%sfont/%s/%s/grid/%s", theme_base, dims[i], curr_lang, section, mux_module);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/%s/grid/default", theme_base, dims[i], curr_lang, section);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/grid/%s", theme_base, dims[i], section, mux_module);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/grid/default", theme_base, dims[i], section);

            font = try_font_at(base, resolved, size);
        }
    }

    if (!font) {
        for (int i = 0; i < 2 && !font; i++) {
            snprintf(base, sizeof(base), "%s/%sfont/%s/%s/%s", theme_base, dims[i], curr_lang, section, mux_module);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/%s/default", theme_base, dims[i], curr_lang, section);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/%s", theme_base, dims[i], section, mux_module);

            if ((font = try_font_at(base, resolved, size))) break;
            snprintf(base, sizeof(base), "%s/%sfont/%s/default", theme_base, dims[i], section);

            font = try_font_at(base, resolved, size);
        }
    }

    if (font) {
        LOG_INFO(mux_module, "Loading Section '%s' Font: %s", section, resolved);
        apply_font(element, font);
        return;
    }

    if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
        apply_font(element, create_language_font(get_section_ttf_size(section)));
    } else {
        lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    }
}

int font_context_changed(void) {
    char context[MAX_BUFFER_SIZE];
    snprintf(context, sizeof(context), "%s|%s|%d|%s|%d|%d|%d|%d",
             config.THEME.ACTIVE,
             config.SETTINGS.GENERAL.LANGUAGE,
             config.SETTINGS.ADVANCED.FONT,
             config.SETTINGS.FONT.NAME,
             config.SETTINGS.FONT.LIST_SIZE,
             config.SETTINGS.FONT.HEADER_SIZE,
             config.SETTINGS.FONT.FOOTER_SIZE,
             config.SETTINGS.FONT.PANEL_SIZE);

    if (strncmp(context, last_font_key, sizeof(last_font_key) - 1) != 0) {
        snprintf(last_font_key, sizeof(last_font_key), "%s", context);
        LOG_INFO(mux_module, "Font context has changed");
        return 1;
    }

    return 0;
}
