#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include "font.h"
#include "../fileio.h"
#include "../content.h"
#include "../init.h"

#include "../theme.h"
#include "../log.h"
#include "../config.h"
#include "../device.h"
#include "../input/list_nav.h"

#define DEFAULT_NAME "Noto Sans"
#define DEFAULT_FONT INTERNAL_FONTS "/" DEFAULT_NAME ".ttf"

// Max TTF file size accepted (64 MB). Protects against accidentally pointing at a
// giant file and exhausting RAM before we've had a chance to log anything useful.
#define TTF_MAX_FILE_BYTES (64 * 1024 * 1024)

// Glyph bitmap cache handed to TinyTTF
// ------------------------------------------------------------
// INTERNAL: Theme and custom TTFs that use a Latin-subset; 512 KB gives
// ~1300 slots at 20 px, which comfortably covers the full ASCII+Latin-1
// range. The cache warms up fully on first scroll and stays in memory.
// ------------------------------------------------------------
// LANGUAGE: User selected languages can have CJK fonts where every filename
// contributes unique glyphs that will never repeat. 1 MB doubles the slot
// count (~2600 slots at 20 px), cutting miss rate noticeably on long lists
// without being reckless with LVGL heap on low memory targets.

static int font_cache_count = 0;
static uint32_t last_font_key_hash = 0;
static int cached_has_theme_font = -1;

// Open address hash table for the font cache
// Must be a power-of-2 and at least 2× FONT_CACHE_MAX so load factor stays ≤ 50%
#define FONT_CACHE_SLOTS 512
_Static_assert(FONT_CACHE_SLOTS >= FONT_CACHE_MAX * 2, "FONT_CACHE_SLOTS must be >= 2 * FONT_CACHE_MAX");
_Static_assert((FONT_CACHE_SLOTS & (FONT_CACHE_SLOTS - 1)) == 0, "FONT_CACHE_SLOTS must be a power of 2");

typedef struct {
    uint32_t hash;
    char path[MAX_BUFFER_SIZE];
    int size;

    lv_font_t *font;

    void *data;
    int is_ttf;
} font_cache_t;

static font_cache_t font_cache[FONT_CACHE_SLOTS];

int theme_has_font(void) {
    if (cached_has_theme_font >= 0) return cached_has_theme_font;

    const char *dims[] = {mux_dim, "", NULL};

    cached_has_theme_font = 0;
    for (int d = 0; dims[d] != NULL; d++) {
        char dir[MAX_BUFFER_SIZE];
        snprintf(dir, sizeof(dir), "%s/%sfont", theme_base, dims[d]);

        struct dirent **entries;
        const int n = scandir(dir, &entries, NULL, NULL);
        if (n < 0) continue;

        int found = 0;
        for (int i = 0; i < n; i++) {
            if (!found) {
                const char *exts[] = {".ttf", ".bin"};
                const char *name = entries[i]->d_name;
                const size_t len = strlen(name);

                for (int e = 0; e < 2; e++) {
                    if (len > 4 && strcasecmp(name + len - 4, exts[e]) == 0) {
                        found = 1;
                        break;
                    }
                }

                if (!found && entries[i]->d_type == DT_DIR && name[0] != '.') {
                    char sub[MAX_BUFFER_SIZE];
                    snprintf(sub, sizeof(sub), "%s/%s", dir, name);

                    struct dirent **sub_entries;
                    const int m = scandir(sub, &sub_entries, NULL, NULL);

                    if (m >= 0) {
                        for (int j = 0; j < m; j++) {
                            if (!found) {
                                const char *s_name = sub_entries[j]->d_name;
                                const size_t s_len = strlen(s_name);
                                for (int e = 0; e < 2; e++) {
                                    if (s_len > 4 && strcasecmp(s_name + s_len - 4, exts[e]) == 0) {
                                        found = 1;
                                        break;
                                    }
                                }
                            }
                            free(sub_entries[j]);
                        }
                        free(sub_entries);
                    }
                }
            }
            free(entries[i]);
        }
        free(entries);

        if (found) {
            cached_has_theme_font = 1;
            break;
        }
    }

    return cached_has_theme_font;
}

static int effective_type(void) {
    if (config.settings.advanced.font == 1 && !theme_has_font()) return 2;
    return config.settings.advanced.font;
}

int get_font_size(void) {
    switch (device.mux.width) {
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
    return theme.font.font_list_size > 0 ? (int) theme.font.font_list_size : get_font_size();
}

static int get_section_ttf_size(const char *section) {
    if (strcmp(section, FONT_HEADER_DIR) == 0 && theme.font.font_header_size > 0) return theme.font.font_header_size;
    if (strcmp(section, FONT_FOOTER_DIR) == 0 && theme.font.font_footer_size > 0) return theme.font.font_footer_size;
    if (strcmp(section, FONT_PANEL_DIR) == 0 && grid_mode_enabled && theme.font.font_panel_size > 0)
        return theme.font.font_panel_size;
    return get_font_size();
}

static int get_custom_section_size(const char *section) {
    if (strcmp(section, FONT_HEADER_DIR) == 0) {
        if (config.settings.font.header_size > 0) return config.settings.font.header_size;
        if (theme.font.font_header_size > 0) return theme.font.font_header_size;

        return get_font_size();
    }

    if (strcmp(section, FONT_FOOTER_DIR) == 0) {
        if (config.settings.font.footer_size > 0) return config.settings.font.footer_size;
        if (theme.font.font_footer_size > 0) return theme.font.font_footer_size;

        return get_font_size();
    }

    if (strcmp(section, FONT_PANEL_DIR) == 0 && grid_mode_enabled) {
        if (config.settings.font.panel_size > 0) return config.settings.font.panel_size;
        if (theme.font.font_panel_size > 0) return theme.font.font_panel_size;

        return get_font_size();
    }

    if (config.settings.font.list_size > 0) return config.settings.font.list_size;
    if (theme.font.font_list_size > 0) return theme.font.font_list_size;

    return get_font_size();
}

static lv_font_t *load_font_cached_ttf_lang(const char *path, int size);
static lv_font_t *load_font_cached_ttf(const char *path, int size, int set_fallback);

static lv_font_t *create_language_font(const int size) {
    const char *curr_lang = config.settings.general.language;
    const char *name = config.settings.font.name;

    if (config.settings.advanced.font == 0 && curr_lang[0] && name[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s/%s.ttf", curr_lang, name);
        lv_font_t *font = load_font_cached_ttf_lang(path, size);
        if (font) return font;
    }

    return NULL;
}

lv_font_t *get_language_font(void) {
    return create_language_font(get_font_size());
}

static lv_font_t *guaranteed_font(const int size) {
    lv_font_t *font = create_language_font(size);
    if (font) return font;

    return load_font_cached_ttf(DEFAULT_FONT, size, 1);
}

void font_cache_clear(void) {
    for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
        if (font_cache[i].path[0] == '\0') continue;

        if (font_cache[i].is_ttf) {
            lv_tiny_ttf_destroy(font_cache[i].font);
            free(font_cache[i].data);
        } else {
            lv_font_free(font_cache[i].font);
        }
    }

    memset(font_cache, 0, sizeof(font_cache));
    font_cache_count = 0;
    cached_has_theme_font = -1;
    LOG_SUCCESS(mux_module, "Font cache has been cleared");
}

static void apply_font(lv_obj_t *element, lv_font_t *font) {
    if (!element || !font) return;
    lv_obj_set_style_text_font(element, font, MU_OBJ_MAIN_DEFAULT);
}

static lv_font_t *load_font_from_bin(const char *filepath) {
    LOG_WARN(mux_module, "BIN font support is deprecated; please use TTF: %s", filepath);

    char fs_path[MAX_BUFFER_SIZE];
    snprintf(fs_path, sizeof(fs_path), "M:%s", filepath);

    lv_font_t *font = lv_font_load(fs_path);
    if (!font) return NULL;

    return font;
}

static void prewarm_ascii(lv_font_t *font) {
    if (!font || !font->get_glyph_dsc || !font->get_glyph_bitmap) return;
    lv_font_glyph_dsc_t dsc;
    for (uint32_t cp = 0x0020; cp <= 0x007E; cp++) {
        if (!font->get_glyph_dsc(font, &dsc, cp, 0)) continue;
        if (!dsc.adv_w || !dsc.box_w || !dsc.box_h) continue;
        font->get_glyph_bitmap(font, cp);
    }
}

static uint32_t font_key_hash(const char *path, const int size) {
    uint32_t h = fnv_hash_str(path);
    h ^= (uint32_t) size;
    h *= 16777619u;

    return h;
}

static lv_font_t *cache_lookup(const char *path, const int size) {
    const uint32_t h = font_key_hash(path, size);
    const uint32_t slot = h & (FONT_CACHE_SLOTS - 1);

    for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
        const font_cache_t *e = &font_cache[(slot + i) & (FONT_CACHE_SLOTS - 1)];
        if (e->path[0] == '\0') return NULL;
        if (e->hash == h && e->size == size && strcmp(e->path, path) == 0) return e->font;
    }

    return NULL;
}

static int cache_store(const char *path, const int size, lv_font_t *font, void *data, const int is_ttf) {
    if (font_cache_count >= FONT_CACHE_MAX) {
        if (is_ttf) {
            lv_tiny_ttf_destroy(font);
            free(data);
        } else {
            lv_font_free(font);
        }
        LOG_WARN(mux_module, "Font cache full; discarding: %s", path);
        return 0;
    }

    const uint32_t h = font_key_hash(path, size);
    const uint32_t slot = h & (FONT_CACHE_SLOTS - 1);

    for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
        font_cache_t *e = &font_cache[(slot + i) & (FONT_CACHE_SLOTS - 1)];

        if (e->path[0] != '\0') continue;
        e->hash = h;

        snprintf(e->path, MAX_BUFFER_SIZE, "%s", path);

        e->size = size;
        e->font = font;
        e->data = data;

        e->is_ttf = is_ttf;
        font_cache_count++;

        return 1;
    }

    LOG_WARN(mux_module, "Font hash table unexpectedly full; discarding: %s", path);
    if (is_ttf) {
        lv_tiny_ttf_destroy(font);
        free(data);
    } else {
        lv_font_free(font);
    }

    return 0;
}

static lv_font_t *load_font_cached_bin(const char *path) {
    lv_font_t *hit = cache_lookup(path, 0);
    if (hit) return hit;

    lv_font_t *font = load_font_from_bin(path);
    if (!font) return NULL;

    if (!cache_store(path, 0, font, NULL, 0)) return NULL;
    font->fallback = get_language_font();

    return font;
}

static lv_font_t *load_ttf_impl(const char *path, int size, int set_fallback) {
    lv_font_t *hit = cache_lookup(path, size);
    if (hit) return hit;

    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_WARN(mux_module, "Cannot open TTF font: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size <= 0 || file_size > TTF_MAX_FILE_BYTES) {
        LOG_WARN(mux_module, "TTF font %s has unexpected size (%ld bytes)", path, file_size);
        fclose(f);
        return NULL;
    }

    void *data = malloc((size_t) file_size);
    if (!data) {
        LOG_ERROR(mux_module, "Out of memory loading TTF font (%ld bytes): %s", file_size, path);
        fclose(f);
        return NULL;
    }

    if ((long) fread(data, 1, (size_t) file_size, f) != file_size) {
        LOG_WARN(mux_module, "Short read on TTF font: %s", path);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    lv_font_t *font = lv_tiny_ttf_create_data_ex(data, (size_t) file_size, size, 524288);
    if (!font) {
        LOG_WARN(mux_module, "TinyTTF failed to parse: %s", path);
        free(data);
        return NULL;
    }

    LOG_INFO(mux_module, "TTF font loaded (%ld KB, glyph cache %d KB): %s", file_size / 1024, 524288 / 1024, path);

    if (!cache_store(path, size, font, data, 1)) return NULL;
    prewarm_ascii(font);

    if (set_fallback) font->fallback = get_language_font();

    return font;
}

static lv_font_t *load_font_cached_ttf(const char *path, const int size, const int set_fallback) {
    return load_ttf_impl(path, size, set_fallback);
}

static lv_font_t *load_font_cached_ttf_lang(const char *path, const int size) {
    return load_ttf_impl(path, size, 0);
}

static lv_font_t *try_font_at(const char *base, char *resolved, const int size) {
    char path[MAX_BUFFER_SIZE];

    snprintf(path, sizeof(path), "%s.ttf", base);
    lv_font_t *f = cache_lookup(path, size);
    if (f) {
        snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
        return f;
    }

    if (file_exist(path)) {
        f = load_font_cached_ttf(path, size, 1);
        if (f) {
            snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
            return f;
        }
    }

    snprintf(path, sizeof(path), "%s.bin", base);
    f = cache_lookup(path, 0);
    if (f) {
        snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
        return f;
    }

    if (file_exist(path)) {
        f = load_font_cached_bin(path);
        if (f) {
            snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
            return f;
        }
    }

    return NULL;
}

lv_font_t *load_font_pass_roller(void) {
    const int size = device.mux.width >= 1280 ? 48 : 32;

    if (config.settings.advanced.font == 2 && config.settings.font.name[0]) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", config.settings.font.name);
        lv_font_t *f = load_font_cached_ttf(path, size, 0);
        if (f) return f;
    }

    return guaranteed_font(size);
}

static void build_font_candidate(
    char *base, const char *dim, const char *lang, const char *section, const int use_grid, const char *name
) {
    const size_t base_size = MAX_BUFFER_SIZE;

    size_t n = (size_t) snprintf(base, base_size, "%s/%sfont", theme_base, dim);
    if (n >= base_size) n = base_size - 1;

    if (lang && lang[0]) {
        n += (size_t) snprintf(base + n, base_size - n, "/%s", lang);
        if (n >= base_size) n = base_size - 1;
    }
    if (section && section[0]) {
        n += (size_t) snprintf(base + n, base_size - n, "/%s", section);
        if (n >= base_size) n = base_size - 1;
    }
    if (use_grid) {
        n += (size_t) snprintf(base + n, base_size - n, "/grid");
        if (n >= base_size) n = base_size - 1;
    }

    snprintf(base + n, base_size - n, "/%s", name);
}

static lv_font_t *
find_theme_font(const char *curr_lang, const char *section, const int use_grid, const int size, char *resolved) {
    char *dims[2] = {mux_dim, ""};
    char base[MAX_BUFFER_SIZE];
    lv_font_t *font = NULL;

    const char *sections[2] = {section, NULL};
    const int section_count = section && section[0] ? 2 : 1;

    for (int s = 0; s < section_count && !font; s++) {
        for (int i = 0; i < 2 && !font; i++) {
            build_font_candidate(base, dims[i], curr_lang, sections[s], use_grid, mux_module);
            if ((font = try_font_at(base, resolved, size))) break;

            build_font_candidate(base, dims[i], curr_lang, sections[s], use_grid, "default");
            if ((font = try_font_at(base, resolved, size))) break;

            build_font_candidate(base, dims[i], NULL, sections[s], use_grid, mux_module);
            if ((font = try_font_at(base, resolved, size))) break;

            build_font_candidate(base, dims[i], NULL, sections[s], use_grid, "default");
            font = try_font_at(base, resolved, size);
        }
    }

    return font;
}

void load_font_text(lv_obj_t *screen) {
    const int eff_type = effective_type();

    int lang_size;
    if (eff_type == 0 && config.settings.font.list_size > 0) {
        lang_size = config.settings.font.list_size;
    } else if (theme.font.font_list_size > 0) {
        lang_size = (int) theme.font.font_list_size;
    } else {
        lang_size = get_font_size();
    }

    if (eff_type == 2) {
        const char *name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;

        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", name);

        int size;
        if (config.settings.font.list_size > 0) {
            size = config.settings.font.list_size;
        } else if (theme.font.font_list_size > 0) {
            size = (int) theme.font.font_list_size;
        } else {
            size = get_font_size();
        }

        lv_font_t *font = load_font_cached_ttf(path, size, 1);

        if (!font && strcmp(name, DEFAULT_NAME) != 0) {
            snprintf(path, sizeof(path), DEFAULT_FONT);
            font = load_font_cached_ttf(path, size, 1);
        }

        if (font) {
            LOG_INFO(mux_module, "Loading Custom Font: %s", path);
            apply_font(screen, font);
            return;
        }
    }

    if (eff_type == 1) {
        const char *curr_lang = config.settings.general.language;
        const int size = get_ttf_size();

        char resolved[MAX_BUFFER_SIZE];
        lv_font_t *font = NULL;

        if (grid_mode_enabled) font = find_theme_font(curr_lang, NULL, 1, size, resolved);
        if (!font) font = find_theme_font(curr_lang, NULL, 0, size, resolved);

        if (font) {
            LOG_INFO(mux_module, "Loading Theme Font: %s", resolved);
            apply_font(screen, font);
            return;
        }
    }

    LOG_INFO(mux_module, "Loading Default Language Font");
    apply_font(screen, guaranteed_font(lang_size));
}

void load_font_section(const char *section, lv_obj_t *element) {
    const int eff_type = effective_type();

    if (eff_type == 2) {
        const char *name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), INTERNAL_FONTS "/%s.ttf", name);

        const int size = get_custom_section_size(section);
        lv_font_t *font = load_font_cached_ttf(path, size, 1);

        if (!font && strcmp(name, DEFAULT_NAME) != 0) {
            snprintf(path, sizeof(path), DEFAULT_FONT);
            font = load_font_cached_ttf(path, size, 1);
        }

        if (font) {
            LOG_INFO(mux_module, "Loading Custom Section '%s' Font: %s", section, path);
            apply_font(element, font);
            return;
        }

        if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
            apply_font(element, guaranteed_font(get_custom_section_size(section)));
        } else {
            lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
        }

        return;
    }

    if (!eff_type) {
        const int size = get_custom_section_size(section);
        if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
            apply_font(element, guaranteed_font(size));
        } else {
            lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
        }

        return;
    }

    const char *curr_lang = config.settings.general.language;
    const int size = get_section_ttf_size(section);

    char resolved[MAX_BUFFER_SIZE];
    lv_font_t *font = NULL;

    if (grid_mode_enabled) font = find_theme_font(curr_lang, section, 1, size, resolved);
    if (!font) font = find_theme_font(curr_lang, section, 0, size, resolved);

    if (font) {
        LOG_INFO(mux_module, "Loading Section '%s' Font: %s", section, resolved);
        apply_font(element, font);
        return;
    }

    if (strcmp(section, FONT_PANEL_DIR) != 0 || grid_mode_enabled) {
        apply_font(element, guaranteed_font(get_section_ttf_size(section)));
    } else {
        lv_obj_remove_local_style_prop(element, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    }
}

int font_context_changed(void) {
    uint32_t h = fnv_hash_str(config.theme.active);

    h ^= fnv_hash_str(config.settings.general.language);
    h *= 16777619u;

    h ^= (uint32_t) config.settings.advanced.font;
    h *= 16777619u;

    if (h == last_font_key_hash) return 0;
    last_font_key_hash = h;

    LOG_INFO(mux_module, "Font context has changed");
    return 1;
}
