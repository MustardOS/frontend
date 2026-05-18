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
#include "../font/notosans_medium.h"
#include "../font/notosans_ar_medium.h"
#include "../font/notosans_jp_medium.h"
#include "../font/notosans_kr_medium.h"
#include "../font/notosans_sc_medium.h"
#include "../font/notosans_tc_medium.h"

// Max TTF file size accepted (64 MB). Protects against accidentally pointing at a
// giant file and exhausting RAM before we've had a chance to log anything useful.
#define TTF_MAX_FILE_BYTES (64 * 1024 * 1024)

// Glyph bitmap cache handed to TinyTTF. Larger = fewer re-rasterisations of the
// same glyph, which matters most for CJK where each character is unique.
#define TTF_GLYPH_CACHE_BYTES (256 * 1024)

int font_cache_count = 0;
char last_font_key[256] = "";

typedef struct {
    const char *lang;
    const void *font;
} font_lang_t;

typedef struct {
    char path[MAX_BUFFER_SIZE];

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

static lv_font_t *create_language_font(int size) {
    size_t cache_size = MAX_BUFFER_SIZE * 10;

    static const font_lang_t font_map[] = {
            {"Chinese (Simplified)",  &notosans_sc_medium_ttf},
            {"Chinese (Traditional)", &notosans_tc_medium_ttf},
            {"Japanese",              &notosans_jp_medium_ttf},
            {"Arabic",                &notosans_ar_medium_ttf},
            {"Korean",                &notosans_kr_medium_ttf},
    };

    const char *curr_lang = config.SETTINGS.GENERAL.LANGUAGE;
    const void *font_data = &notosans_medium_ttf;

    for (size_t i = 0; i < sizeof(font_map) / sizeof(font_map[0]); i++) {
        if (strcasecmp(curr_lang, font_map[i].lang) == 0) {
            font_data = font_map[i].font;
            break;
        }
    }

    return lv_tiny_ttf_create_data_ex(font_data, notosans_medium_ttf_len, size, cache_size);
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

    font->fallback = get_language_font();
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

    font->fallback = get_language_font();
    LOG_INFO(mux_module, "TTF font loaded into memory (%ld KB): %s", file_size / 1024, filepath);

    *out_data = data;
    return font;
}

static lv_font_t *cache_lookup(const char *path) {
    for (int i = 0; i < font_cache_count; i++) {
        if (strcmp(font_cache[i].path, path) == 0) return font_cache[i].font;
    }

    return NULL;
}

static void cache_store(const char *path, lv_font_t *font, void *data, int is_ttf) {
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

    strncpy(font_cache[font_cache_count].path, path, MAX_BUFFER_SIZE - 1);

    font_cache[font_cache_count].path[MAX_BUFFER_SIZE - 1] = '\0';
    font_cache[font_cache_count].font = font;
    font_cache[font_cache_count].data = data;
    font_cache[font_cache_count].is_ttf = is_ttf;

    font_cache_count++;
}

static lv_font_t *load_font_cached_bin(const char *path) {
    lv_font_t * hit = cache_lookup(path);
    if (hit) return hit;

    lv_font_t * font = load_font_from_bin(path);
    if (font) cache_store(path, font, NULL, 0);

    return font;
}

static lv_font_t *load_font_cached_ttf(const char *path, int size) {
    lv_font_t * hit = cache_lookup(path);
    if (hit) return hit;

    void *data = NULL;

    lv_font_t * font = load_font_from_ttf(path, size, &data);
    if (font) cache_store(path, font, data, 1);

    return font;
}

static lv_font_t *try_font_at(const char *base, char *resolved, int size) {
    char path[MAX_BUFFER_SIZE];

    snprintf(path, sizeof(path), "%s.ttf", base);
    if (file_exist(path)) {
        lv_font_t * f = load_font_cached_ttf(path, size);
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

void load_font_text(lv_obj_t *screen) {
    lv_font_t * language_font = get_language_font();

    if (config.SETTINGS.ADVANCED.FONT) {
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
    lv_obj_set_style_text_font(screen, language_font, MU_OBJ_MAIN_DEFAULT);
}

void load_font_section(const char *section, lv_obj_t *element) {
    if (!config.SETTINGS.ADVANCED.FONT) return;

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
    }
}

int font_context_changed(void) {
    char context[MAX_BUFFER_SIZE];
    snprintf(context, sizeof(context), "%s|%s", config.THEME.ACTIVE, config.SETTINGS.GENERAL.LANGUAGE);

    if (strcmp(context, last_font_key) != 0) {
        strcpy(last_font_key, context);
        LOG_INFO(mux_module, "Font context has changed");
        return 1;
    }

    return 0;
}
