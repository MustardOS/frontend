#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include <common/ui/font.h>
#include <common/ui/font_info.h>
#include <common/runtime/perf.h>
#include <common/storage/fileio.h>
#include <common/content/content.h>
#include <common/runtime/init.h>
#include <common/display/theme.h>
#include <common/runtime/log.h>
#include <common/config/config.h>
#include <common/platform/device.h>
#include <common/base/strutil.h>
#include <common/ui/navigation/list_nav.h>
#include <harfbuzz/hb.h>

#define DEFAULT_NAME DEFAULT_FONT_NAME
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
static int cached_theme_font_scalable = -1;
static int cached_theme_font_compiled = -1;
static int cached_has_theme_font = -1;
static int cached_user_font_count = -1;

enum script_font {
    SCRIPT_FONT_BASE,
    SCRIPT_FONT_ARABIC,
    SCRIPT_FONT_CHINESE_SIMPLIFIED,
    SCRIPT_FONT_CHINESE_TRADITIONAL,
    SCRIPT_FONT_JAPANESE,
    SCRIPT_FONT_KOREAN,
    SCRIPT_FONT_COUNT
};

typedef struct script_fallback {
    lv_font_t font;
    lv_font_t *loaded[SCRIPT_FONT_COUNT];
    int size;
    struct script_fallback *next;
} script_fallback_t;

static script_fallback_t *script_fallbacks;

// Open address hash table for the font cache
// Must be a power-of-2 and at least 2× FONT_CACHE_MAX so load factor stays ≤ 50%
#define FONT_CACHE_SLOTS 512
_Static_assert(FONT_CACHE_SLOTS >= FONT_CACHE_MAX * 2, "FONT_CACHE_SLOTS must be >= 2 * FONT_CACHE_MAX");
_Static_assert((FONT_CACHE_SLOTS & (FONT_CACHE_SLOTS - 1)) == 0, "FONT_CACHE_SLOTS must be a power of 2");

typedef struct {
    int16_t weight;
    int16_t width;
    int16_t slant;
    int16_t italic;
} font_variations_t;

typedef struct {
    uint32_t hash;
    char *path;
    int size;
    unsigned int face_index;
    uint64_t variation_key;
    font_variations_t variations;

    lv_font_t *font;
    int is_ttf;
} font_cache_t;

static font_cache_t *font_cache;

typedef struct font_blob {
    char *path;
    void *data;
    size_t size;
    struct font_blob *next;
} font_blob_t;

static font_blob_t *font_blobs;

static const font_variations_t no_font_variations;

static int supported_font_extension(const char *name) {
    const size_t length = name ? strlen(name) : 0;
    if (length < 4) return 0;

    const char *extension = name + length - 4;
    return strcasecmp(extension, ".ttf") == 0 || strcasecmp(extension, ".otf") == 0
           || strcasecmp(extension, ".ttc") == 0 || strcasecmp(extension, ".pcf") == 0
           || strcasecmp(extension, ".bdf") == 0;
}

static void font_path_join(char *out, const size_t out_size, const char *directory, const char *name) {
    if (supported_font_extension(name))
        snprintf(out, out_size, "%s/%s", directory, name);
    else
        snprintf(out, out_size, "%s/%s.ttf", directory, name);
}

static int theme_font_scan_at(const char *base, const int mode) {

    const char *dims[] = {mux_dim, "", NULL};

    for (int d = 0; dims[d] != NULL; d++) {
        char dir[MAX_BUFFER_SIZE];
        snprintf(dir, sizeof(dir), "%s/%sfont", base, dims[d]);

        struct dirent **entries;
        const int n = scandir(dir, &entries, NULL, NULL);
        if (n < 0) continue;

        int found = 0;
        for (int i = 0; i < n; i++) {
            if (!found) {
                const char *exts[] = {".ttf", ".otf", ".ttc", ".pcf", ".bdf", ".bin"};
                const int ext_first = mode == 2 ? 5 : 0;
                const int ext_count = mode == 1 ? 3 : 6;
                const char *name = entries[i]->d_name;
                const size_t len = strlen(name);

                for (int e = ext_first; e < ext_count; e++) {
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
                                for (int e = ext_first; e < ext_count; e++) {
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

        if (found) return 1;
    }

    return 0;
}

int user_font_path(const char *name, char *out, const size_t out_size) {
    if (!name || !*name) return 0;

    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); i++) {
        if (!mounts[i] || !*mounts[i]) continue;

        char directory[MAX_BUFFER_SIZE];
        snprintf(directory, sizeof(directory), "%s/%s", mounts[i], USER_FONTS);
        font_path_join(out, out_size, directory, name);
        remove_double_slashes(out);

        if (file_exist_nocase(out, out, out_size)) return 1;
    }

    return 0;
}

int user_font_count(void) {
    if (cached_user_font_count >= 0) return cached_user_font_count;

    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    int total = 0;
    for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); i++) {
        if (!mounts[i] || !*mounts[i]) continue;

        char dir[MAX_BUFFER_SIZE];
        snprintf(dir, sizeof(dir), "%s/%s", mounts[i], USER_FONTS);
        remove_double_slashes(dir);

        struct dirent **entries;
        const int n = scandir(dir, &entries, NULL, alphasort);
        if (n < 0) continue;

        for (int e = 0; e < n; e++) {
            const char *name = entries[e]->d_name;

            if (supported_font_extension(name)) {
                total++;
                free(entries[e]);
                continue;
            }

            if (name[0] != '.') {
                char nested[MAX_BUFFER_SIZE];
                snprintf(nested, sizeof(nested), "%s/%s", dir, name);

                struct dirent **variants;
                const int vn = scandir(nested, &variants, NULL, alphasort);

                for (int v = 0; v < vn; v++) {
                    const size_t vlen = strlen(variants[v]->d_name);
                    if (vlen > 4 && supported_font_extension(variants[v]->d_name)) total++;

                    free(variants[v]);
                }

                if (vn >= 0) free(variants);
            }

            free(entries[e]);
        }

        free(entries);
    }

    cached_user_font_count = total;

    return total;
}

int theme_has_font(void) {
    if (cached_has_theme_font < 0) cached_has_theme_font = theme_font_scan_at(theme_base, 0);

    return cached_has_theme_font;
}

int theme_font_is_scalable(void) {
    if (cached_theme_font_scalable < 0) cached_theme_font_scalable = theme_font_scan_at(theme_base, 1);

    return cached_theme_font_scalable;
}

int theme_font_is_compiled(void) {
    if (cached_theme_font_compiled < 0) cached_theme_font_compiled = theme_font_scan_at(theme_base, 2);

    return cached_theme_font_compiled;
}

int theme_path_has_font(const char *path) {
    return path && *path && theme_font_scan_at(path, 0);
}

static int effective_type(void) {
    if (config.settings.advanced.font == 1 && !theme_has_font()) return 2;
    if (config.settings.advanced.font == 3 && user_font_count() == 0) return 2;
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

static int scale_font_size(const int size) {
    const int scale = config.settings.font.scale;
    if (scale <= 0 || scale == 100) return size;

    return size * scale / 100;
}

static int get_unscaled_section_size(const char *section) {
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

static int get_custom_section_size(const char *section) {
    return scale_font_size(get_unscaled_section_size(section));
}

static lv_font_t *load_font_cached_ttf_lang(const char *path, int size);
static lv_font_t *load_font_cached_ttf(const char *path, int size, int set_fallback);
static lv_font_t *load_font_cached_face(const char *path, int size, int set_fallback, unsigned int face_index);
static lv_font_t *get_script_fallback(int size);

static const char *font_leaf_name(const char *name) {
    if (!name) return "";

    const char *leaf = strrchr(name, '/');
    return leaf ? leaf + 1 : name;
}

static unsigned int configured_font_face(void) {
    return config.settings.font.face >= 0 ? (unsigned int) config.settings.font.face : 0;
}

static font_variations_t configured_font_variations(void) {
    return (font_variations_t) {.width = config.settings.font.width, .italic = config.settings.font.italic};
}

static uint64_t font_variation_key(const font_variations_t *variations) {
    uint64_t key = 1469598103934665603ull;
    const unsigned char *bytes = (const unsigned char *) variations;
    for (size_t i = 0; i < sizeof(*variations); i++) {
        key ^= bytes[i];
        key *= 1099511628211ull;
    }
    return key;
}

static uint32_t font_variation_axes(const font_variations_t *variations, lv_tiny_ttf_axis_t axes[static 4]) {
    uint32_t count = 0;
    if (variations->weight)
        axes[count++] = (lv_tiny_ttf_axis_t) {.tag = HB_TAG('w', 'g', 'h', 't'), .value = variations->weight};
    if (variations->width)
        axes[count++] = (lv_tiny_ttf_axis_t) {.tag = HB_TAG('w', 'd', 't', 'h'), .value = variations->width};
    if (variations->slant)
        axes[count++] = (lv_tiny_ttf_axis_t) {.tag = HB_TAG('s', 'l', 'n', 't'), .value = variations->slant};
    if (variations->italic)
        axes[count++] = (lv_tiny_ttf_axis_t) {.tag = HB_TAG('i', 't', 'a', 'l'), .value = variations->italic};
    return count;
}

static void configured_font_reference(char *out, const size_t out_size) {
    const char *name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;

    if (config.settings.font.directory[0]) {
        snprintf(out, out_size, "%s/%s", config.settings.font.directory, font_leaf_name(name));
    } else {
        snprintf(out, out_size, "%s", name);
    }
}

static int configured_font_path(char *out, const size_t out_size) {
    const int type = effective_type();
    const char *name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;

    if (type == 3) {
        char reference[MAX_BUFFER_SIZE];
        configured_font_reference(reference, sizeof(reference));
        return user_font_path(reference, out, out_size);
    }

    if (type == 0 && config.settings.font.directory[0]) {
        char directory[MAX_BUFFER_SIZE];
        snprintf(directory, sizeof(directory), INTERNAL_FONTS "/%s", config.settings.font.directory);
        font_path_join(out, out_size, directory, font_leaf_name(name));
        return file_exist_nocase(out, out, out_size);
    }

    font_path_join(out, out_size, INTERNAL_FONTS, font_leaf_name(name));
    return file_exist_nocase(out, out, out_size);
}

static lv_font_t *create_language_font(const int size) {
    const char *curr_lang = config.settings.general.language;
    const char *name = config.settings.font.name;

    if (config.settings.advanced.font == 0 && name[0]) {
        char path[MAX_BUFFER_SIZE];
        if (config.settings.font.directory[0]) {
            char directory[MAX_BUFFER_SIZE];
            snprintf(directory, sizeof(directory), INTERNAL_FONTS "/%s", config.settings.font.directory);
            font_path_join(path, sizeof(path), directory, font_leaf_name(name));
        } else if (strchr(name, '/')) {
            font_path_join(path, sizeof(path), INTERNAL_FONTS, name);
        } else {
            char directory[MAX_BUFFER_SIZE];
            snprintf(directory, sizeof(directory), INTERNAL_FONTS "/%s", curr_lang);
            font_path_join(path, sizeof(path), directory, name);
        }
        lv_font_t *font = load_font_cached_face(path, size, 0, configured_font_face());
        if (font) {
            font->fallback = get_script_fallback(size);
            return font;
        }
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

static font_blob_t *font_blob_load(const char *path) {
    for (font_blob_t *blob = font_blobs; blob; blob = blob->next)
        if (strcmp(blob->path, path) == 0) return blob;

    FILE *file = fopen(path, "rb");
    if (!file) {
        LOG_WARN(mux_module, "Cannot open font: %s", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long file_size = ftell(file);
    rewind(file);
    if (file_size <= 0 || file_size > TTF_MAX_FILE_BYTES) {
        LOG_WARN(mux_module, "Font %s has unexpected size (%ld bytes)", path, file_size);
        fclose(file);
        return NULL;
    }

    font_blob_t *blob = calloc(1, sizeof(*blob));
    if (!blob) {
        fclose(file);
        return NULL;
    }
    blob->data = malloc((size_t) file_size);
    blob->path = strdup(path);
    if (!blob->data || !blob->path || fread(blob->data, 1, (size_t) file_size, file) != (size_t) file_size) {
        free(blob->path);
        free(blob->data);
        free(blob);
        fclose(file);
        return NULL;
    }
    fclose(file);

    blob->size = (size_t) file_size;
    blob->next = font_blobs;
    font_blobs = blob;
    return blob;
}

void font_cache_clear(void) {
    while (script_fallbacks) {
        script_fallback_t *next = script_fallbacks->next;
        free(script_fallbacks);
        script_fallbacks = next;
    }

    if (font_cache) {
        for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
            if (!font_cache[i].path) continue;

            if (font_cache[i].is_ttf) {
                lv_tiny_ttf_destroy(font_cache[i].font);
            } else {
                lv_font_free(font_cache[i].font);
            }
            free(font_cache[i].path);
        }

        free(font_cache);
        font_cache = NULL;
    }

    while (font_blobs) {
        font_blob_t *next = font_blobs->next;
        free(font_blobs->path);
        free(font_blobs->data);
        free(font_blobs);
        font_blobs = next;
    }

    font_cache_count = 0;
    cached_theme_font_scalable = -1;
    cached_theme_font_compiled = -1;
    cached_has_theme_font = -1;
    cached_user_font_count = -1;
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

static uint32_t
font_key_hash(const char *path, const int size, const unsigned int face_index, const uint64_t variation_key) {
    uint32_t h = fnv_hash_str(path);
    h ^= (uint32_t) size;
    h *= 16777619u;
    h ^= face_index;
    h *= 16777619u;
    h ^= (uint32_t) variation_key;
    h *= 16777619u;
    h ^= (uint32_t) (variation_key >> 32u);
    h *= 16777619u;

    return h;
}

static lv_font_t *
cache_lookup(const char *path, const int size, const unsigned int face_index, const uint64_t variation_key) {
    if (!font_cache) return NULL;

    const uint32_t h = font_key_hash(path, size, face_index, variation_key);
    const uint32_t slot = h & (FONT_CACHE_SLOTS - 1);

    for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
        const font_cache_t *e = &font_cache[(slot + i) & (FONT_CACHE_SLOTS - 1)];
        if (!e->path) return NULL;
        if (e->hash == h && e->size == size && e->face_index == face_index && e->variation_key == variation_key
            && strcmp(e->path, path) == 0)
            return e->font;
    }

    return NULL;
}

static int cache_store(
    const char *path, const int size, const unsigned int face_index, const uint64_t variation_key, lv_font_t *font,
    const int is_ttf, const font_variations_t *variations
) {
    if (font_cache_count >= FONT_CACHE_MAX) {
        if (is_ttf) {
            lv_tiny_ttf_destroy(font);
        } else {
            lv_font_free(font);
        }
        LOG_WARN(mux_module, "Font cache full; discarding: %s", path);
        return 0;
    }

    if (!font_cache) {
        font_cache = calloc(FONT_CACHE_SLOTS, sizeof(*font_cache));
        if (!font_cache) {
            if (is_ttf) {
                lv_tiny_ttf_destroy(font);
            } else {
                lv_font_free(font);
            }
            return 0;
        }
    }

    const uint32_t h = font_key_hash(path, size, face_index, variation_key);
    const uint32_t slot = h & (FONT_CACHE_SLOTS - 1);

    for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
        font_cache_t *e = &font_cache[(slot + i) & (FONT_CACHE_SLOTS - 1)];

        if (e->path) continue;
        e->hash = h;
        e->path = strdup(path);
        if (!e->path) {
            memset(e, 0, sizeof(*e));
            if (is_ttf) {
                lv_tiny_ttf_destroy(font);
            } else {
                lv_font_free(font);
            }
            return 0;
        }

        e->size = size;
        e->face_index = face_index;
        e->variation_key = variation_key;
        if (variations) e->variations = *variations;
        e->font = font;

        e->is_ttf = is_ttf;
        font_cache_count++;

        return 1;
    }

    LOG_WARN(mux_module, "Font hash table unexpectedly full; discarding: %s", path);
    if (is_ttf) {
        lv_tiny_ttf_destroy(font);
    } else {
        lv_font_free(font);
    }

    return 0;
}

static lv_font_t *load_font_cached_bin(const char *path) {
    lv_font_t *hit = cache_lookup(path, 0, 0, 0);
    if (hit) return hit;

    lv_font_t *font = load_font_from_bin(path);
    if (!font) return NULL;

    if (!cache_store(path, 0, 0, 0, font, 0, NULL)) return NULL;
    font->fallback = get_script_fallback(font->line_height);

    return font;
}

static lv_font_t *load_ttf_impl(
    const char *path, const int size, const int set_fallback, const unsigned int face_index,
    const font_variations_t *variations
) {
    const uint64_t variation_key = font_variation_key(variations);
    lv_font_t *hit = cache_lookup(path, size, face_index, variation_key);
    if (hit) return hit;

    font_blob_t *blob = font_blob_load(path);
    if (!blob) return NULL;

    size_t glyph_cache_size = 128 * 1024;
    if (size > 28) glyph_cache_size = 256 * 1024;
    if (size > 48) glyph_cache_size = 512 * 1024;

    lv_font_t *font = lv_tiny_ttf_create_data_face_ex(blob->data, blob->size, size, glyph_cache_size, face_index);
    if (!font) {
        LOG_WARN(mux_module, "TinyTTF failed to parse: %s", path);
        return NULL;
    }

    lv_tiny_ttf_axis_t axes[4];
    const uint32_t axis_count = font_variation_axes(variations, axes);
    if (axis_count > 0 && !lv_tiny_ttf_set_variations(font, axes, axis_count)) {
        LOG_WARN(mux_module, "Font variations could not be applied: %s", path);
    }

    LOG_INFO(
        mux_module, "Font loaded (%zu KB, face %u, glyph cache %zu KB): %s", blob->size / 1024, face_index,
        glyph_cache_size / 1024, path
    );

    if (!cache_store(path, size, face_index, variation_key, font, 1, variations)) return NULL;
    prewarm_ascii(font);

    if (set_fallback) font->fallback = get_script_fallback(size);

    return font;
}

static lv_font_t *load_font_cached_ttf(const char *path, const int size, const int set_fallback) {
    return load_ttf_impl(path, size, set_fallback, 0, &no_font_variations);
}

static lv_font_t *
load_font_cached_face(const char *path, const int size, const int set_fallback, const unsigned int face_index) {
    const font_variations_t variations = configured_font_variations();
    return load_ttf_impl(path, size, set_fallback, face_index, &variations);
}

static lv_font_t *load_font_cached_ttf_lang(const char *path, const int size) {
    return load_ttf_impl(path, size, 0, 0, &no_font_variations);
}

static font_cache_t *font_cache_entry(const lv_font_t *font) {
    if (!font_cache || !font) return NULL;
    for (int i = 0; i < FONT_CACHE_SLOTS; i++)
        if (font_cache[i].path && font_cache[i].font == font) return &font_cache[i];
    return NULL;
}

static int
axis_value_for_style(const char *path, const unsigned int face, const uint32_t tag, const int current, const int bold) {
    font_axis_info_t axis;
    if (!font_info_axis(path, face, tag, &axis)) return current;

    float value = current ? current : axis.default_value;
    if (bold) {
        if (value < 700.0f) value = 700.0f;
        if (value > axis.maximum) value = axis.maximum;
        if (value < axis.minimum) value = axis.minimum;
    } else {
        value = axis.minimum < axis.default_value ? axis.minimum : axis.maximum;
    }
    return (int) lroundf(value);
}

static unsigned int
fixed_style_face(const char *path, const unsigned int current_face, const int bold, const int italic) {
    font_info_t base;
    if (!font_info_read(path, current_face, &base) || base.face_count < 2) return current_face;

    for (unsigned int face = 0; face < base.face_count; face++) {
        font_info_t candidate;
        if (!font_info_read(path, face, &candidate) || strcasecmp(candidate.family, base.family) != 0) continue;
        const int candidate_bold =
            strcasestr(candidate.style, "bold") != NULL || strcasestr(candidate.style, "black") != NULL;
        const int candidate_italic =
            strcasestr(candidate.style, "italic") != NULL || strcasestr(candidate.style, "oblique") != NULL;
        if (candidate_bold == !!bold && candidate_italic == !!italic) return face;
    }

    return current_face;
}

lv_font_t *font_style_variant(const lv_font_t *base, const int bold, const int italic) {
    if (!base || (!bold && !italic)) return (lv_font_t *) base;

    font_cache_t *entry = font_cache_entry(base);
    if (!entry || !entry->is_ttf) return (lv_font_t *) base;

    font_variations_t variations = entry->variations;
    int variable = 0;
    font_axis_info_t axis;

    if (bold && font_info_axis(entry->path, entry->face_index, HB_TAG('w', 'g', 'h', 't'), &axis)) {
        variations.weight = (int16_t) axis_value_for_style(
            entry->path, entry->face_index, HB_TAG('w', 'g', 'h', 't'), variations.weight, 1
        );
        variable = 1;
    }
    if (italic && font_info_axis(entry->path, entry->face_index, HB_TAG('i', 't', 'a', 'l'), &axis)) {
        variations.italic = 1;
        variable = 1;
    } else if (italic && font_info_axis(entry->path, entry->face_index, HB_TAG('s', 'l', 'n', 't'), &axis)) {
        variations.slant = (int16_t) axis_value_for_style(
            entry->path, entry->face_index, HB_TAG('s', 'l', 'n', 't'), variations.slant, 0
        );
        variable = 1;
    }

    const unsigned int face =
        variable ? entry->face_index : fixed_style_face(entry->path, entry->face_index, bold, italic);
    if (!variable && face == entry->face_index) return (lv_font_t *) base;

    lv_font_t *variant = load_ttf_impl(entry->path, entry->size, 1, face, &variations);
    return variant ? variant : (lv_font_t *) base;
}

static enum script_font cjk_fallback(void) {
    const char *language = config.settings.general.language;

    if (strcasecmp(language, "Chinese (Traditional)") == 0) return SCRIPT_FONT_CHINESE_TRADITIONAL;
    if (strcasecmp(language, "Japanese") == 0) return SCRIPT_FONT_JAPANESE;
    if (strcasecmp(language, "Korean") == 0) return SCRIPT_FONT_KOREAN;
    return SCRIPT_FONT_CHINESE_SIMPLIFIED;
}

static enum script_font script_for_codepoint(const uint32_t codepoint) {
    if ((codepoint >= 0x0600 && codepoint <= 0x06ff) || (codepoint >= 0x0750 && codepoint <= 0x077f)
        || (codepoint >= 0x0870 && codepoint <= 0x089f) || (codepoint >= 0x08a0 && codepoint <= 0x08ff)
        || (codepoint >= 0xfb50 && codepoint <= 0xfdff) || (codepoint >= 0xfe70 && codepoint <= 0xfeff)
        || (codepoint >= 0x10e60 && codepoint <= 0x10e7f) || (codepoint >= 0x10ec0 && codepoint <= 0x10eff))
        return SCRIPT_FONT_ARABIC;

    if ((codepoint >= 0x1100 && codepoint <= 0x11ff) || (codepoint >= 0x3130 && codepoint <= 0x318f)
        || (codepoint >= 0xa960 && codepoint <= 0xa97f) || (codepoint >= 0xac00 && codepoint <= 0xd7af)
        || (codepoint >= 0xd7b0 && codepoint <= 0xd7ff))
        return SCRIPT_FONT_KOREAN;

    if ((codepoint >= 0x3040 && codepoint <= 0x30ff) || (codepoint >= 0x31f0 && codepoint <= 0x31ff)
        || (codepoint >= 0xff65 && codepoint <= 0xff9f))
        return SCRIPT_FONT_JAPANESE;

    if ((codepoint >= 0x2e80 && codepoint <= 0x2fff) || (codepoint >= 0x3100 && codepoint <= 0x312f)
        || (codepoint >= 0x31a0 && codepoint <= 0x31bf) || (codepoint >= 0x3400 && codepoint <= 0x4dbf)
        || (codepoint >= 0x4e00 && codepoint <= 0x9fff) || (codepoint >= 0xf900 && codepoint <= 0xfaff)
        || (codepoint >= 0x20000 && codepoint <= 0x323af))
        return cjk_fallback();

    return SCRIPT_FONT_BASE;
}

static lv_font_t *script_font_load(script_fallback_t *fallback, const enum script_font script) {
    if (fallback->loaded[script]) return fallback->loaded[script];

    static const char *const paths[SCRIPT_FONT_COUNT] = {
        [SCRIPT_FONT_BASE] = INTERNAL_FONTS "/Noto Sans.ttf",
        [SCRIPT_FONT_ARABIC] = INTERNAL_FONTS "/Arabic/Noto Sans AR.ttf",
        [SCRIPT_FONT_CHINESE_SIMPLIFIED] = INTERNAL_FONTS "/Chinese (Simplified)/Noto Sans SC.ttf",
        [SCRIPT_FONT_CHINESE_TRADITIONAL] = INTERNAL_FONTS "/Chinese (Traditional)/Noto Sans TC.ttf",
        [SCRIPT_FONT_JAPANESE] = INTERNAL_FONTS "/Japanese/Noto Sans JP.ttf",
        [SCRIPT_FONT_KOREAN] = INTERNAL_FONTS "/Korean/Noto Sans KR.ttf"
    };

    fallback->loaded[script] = load_font_cached_ttf_lang(paths[script], fallback->size);
    return fallback->loaded[script];
}

static lv_font_t *script_font_resolve(script_fallback_t *fallback, const uint32_t codepoint) {
    const enum script_font script = script_for_codepoint(codepoint);
    lv_font_t *font = script_font_load(fallback, script);
    lv_font_glyph_dsc_t glyph;

    if (font && font->get_glyph_dsc(font, &glyph, codepoint, 0)) return font;
    if (script == SCRIPT_FONT_BASE) return NULL;

    font = script_font_load(fallback, SCRIPT_FONT_BASE);
    return font && font->get_glyph_dsc(font, &glyph, codepoint, 0) ? font : NULL;
}

static int script_fallback_get_glyph_dsc(
    const lv_font_t *font, lv_font_glyph_dsc_t *out, const uint32_t codepoint, const uint32_t next_codepoint
) {
    script_fallback_t *fallback = (script_fallback_t *) font->dsc;
    lv_font_t *resolved = script_font_resolve(fallback, codepoint);
    if (!resolved) return 0;

    uint32_t resolved_next = 0;
    if (next_codepoint) {
        lv_font_glyph_dsc_t next;
        if (resolved->get_glyph_dsc(resolved, &next, next_codepoint, 0)) resolved_next = next_codepoint;
    }

    return resolved->get_glyph_dsc(resolved, out, codepoint, resolved_next);
}

static const uint8_t *script_fallback_get_glyph_bitmap(const lv_font_t *font, const uint32_t codepoint) {
    script_fallback_t *fallback = (script_fallback_t *) font->dsc;
    lv_font_t *resolved = script_font_resolve(fallback, codepoint);
    return resolved ? resolved->get_glyph_bitmap(resolved, codepoint) : NULL;
}

static lv_font_t *get_script_fallback(const int size) {
    for (script_fallback_t *fallback = script_fallbacks; fallback; fallback = fallback->next)
        if (fallback->size == size) return &fallback->font;

    script_fallback_t *fallback = calloc(1, sizeof(*fallback));
    if (!fallback) return NULL;

    fallback->size = size;
    fallback->font.get_glyph_dsc = script_fallback_get_glyph_dsc;
    fallback->font.get_glyph_bitmap = script_fallback_get_glyph_bitmap;
    fallback->font.line_height = (lv_coord_t) size;
    fallback->font.dsc = fallback;
    fallback->next = script_fallbacks;
    script_fallbacks = fallback;

    return &fallback->font;
}

static lv_font_t *try_font_at(const char *base, char *resolved, const int size) {
    char path[MAX_BUFFER_SIZE];

    static const char *const extensions[] = {".ttf", ".otf", ".ttc", ".pcf", ".bdf"};
    lv_font_t *f = NULL;
    for (size_t i = 0; i < A_SIZE(extensions); i++) {
        snprintf(path, sizeof(path), "%s%s", base, extensions[i]);
        f = cache_lookup(path, size, 0, font_variation_key(&no_font_variations));
        if (f) {
            snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
            return f;
        }

        if (file_exist_nocase(path, path, sizeof(path))) {
            f = load_font_cached_ttf(path, size, 1);
            if (f) {
                snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
                return f;
            }
        }
    }

    snprintf(path, sizeof(path), "%s.bin", base);
    f = cache_lookup(path, 0, 0, 0);
    if (f) {
        snprintf(resolved, MAX_BUFFER_SIZE, "%s", path);
        return f;
    }

    if (file_exist_nocase(path, path, sizeof(path))) {
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
        font_path_join(path, sizeof(path), INTERNAL_FONTS, font_leaf_name(config.settings.font.name));
        lv_font_t *f = load_font_cached_face(path, size, 0, configured_font_face());
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

static void load_font_text_inner(lv_obj_t *screen) {
    const int eff_type = effective_type();

    int lang_size;
    if (config.settings.font.list_size > 0) {
        lang_size = config.settings.font.list_size;
    } else if (theme.font.font_list_size > 0) {
        lang_size = (int) theme.font.font_list_size;
    } else {
        lang_size = get_font_size();
    }
    lang_size = scale_font_size(lang_size);

    if (eff_type == 2 || eff_type == 3) {
        const char *stored_name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;
        const char *name = eff_type == 2 ? font_leaf_name(stored_name) : stored_name;

        char path[MAX_BUFFER_SIZE];
        char reference[MAX_BUFFER_SIZE];
        if (eff_type == 3) configured_font_reference(reference, sizeof(reference));
        if (eff_type != 3 || !user_font_path(reference, path, sizeof(path)))
            font_path_join(path, sizeof(path), INTERNAL_FONTS, name);

        int size;
        if (config.settings.font.list_size > 0) {
            size = config.settings.font.list_size;
        } else if (theme.font.font_list_size > 0) {
            size = (int) theme.font.font_list_size;
        } else {
            size = get_font_size();
        }
        size = scale_font_size(size);

        lv_font_t *font = load_font_cached_face(path, size, 1, configured_font_face());

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

void load_font_text(lv_obj_t *screen) {
    const uint64_t font_start = fe_perf_begin();
    load_font_text_inner(screen);
    fe_perf_end(fe_perf_stage_font, font_start);
}

void load_font_section(const char *section, lv_obj_t *element) {
    const int eff_type = effective_type();

    if (eff_type == 2 || eff_type == 3) {
        const char *stored_name = config.settings.font.name[0] ? config.settings.font.name : DEFAULT_NAME;
        const char *name = eff_type == 2 ? font_leaf_name(stored_name) : stored_name;
        char path[MAX_BUFFER_SIZE];
        char reference[MAX_BUFFER_SIZE];
        if (eff_type == 3) configured_font_reference(reference, sizeof(reference));
        if (eff_type != 3 || !user_font_path(reference, path, sizeof(path)))
            font_path_join(path, sizeof(path), INTERNAL_FONTS, name);

        const int size = get_custom_section_size(section);
        lv_font_t *font = load_font_cached_face(path, size, 1, configured_font_face());

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

    h ^= fnv_hash_str(config.settings.font.name);
    h *= 16777619u;

    h ^= fnv_hash_str(config.settings.font.directory);
    h *= 16777619u;

    h ^= (uint32_t) config.settings.font.face;
    h *= 16777619u;

    const int16_t variations[] = {config.settings.font.width, config.settings.font.italic};
    for (size_t i = 0; i < A_SIZE(variations); i++) {
        h ^= (uint16_t) variations[i];
        h *= 16777619u;
    }

    const int16_t sizes[] = {
        config.settings.font.list_size, config.settings.font.header_size, config.settings.font.footer_size,
        config.settings.font.panel_size
    };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        h ^= (uint32_t) sizes[i];
        h *= 16777619u;
    }

    char path[MAX_BUFFER_SIZE];
    if (configured_font_path(path, sizeof(path))) {
        struct stat info;
        if (stat(path, &info) == 0) {
            h ^= (uint32_t) info.st_size;
            h *= 16777619u;
            h ^= (uint32_t) info.st_mtim.tv_sec;
            h *= 16777619u;
            h ^= (uint32_t) info.st_mtim.tv_nsec;
            h *= 16777619u;
        }
    } else if (effective_type() == 1) {
        struct stat info;
        if (stat(theme_base, &info) == 0) {
            h ^= (uint32_t) info.st_mtim.tv_sec;
            h *= 16777619u;
            h ^= (uint32_t) info.st_mtim.tv_nsec;
            h *= 16777619u;
        }

        if (font_cache) {
            const size_t theme_length = strlen(theme_base);
            for (int i = 0; i < FONT_CACHE_SLOTS; i++) {
                const char *c_path = font_cache[i].path;
                if (!c_path || strncmp(c_path, theme_base, theme_length) != 0
                    || (c_path[theme_length] != '/' && c_path[theme_length] != '\0'))
                    continue;

                if (stat(c_path, &info) == 0) {
                    h ^= (uint32_t) info.st_size;
                    h *= 16777619u;
                    h ^= (uint32_t) info.st_mtim.tv_sec;
                    h *= 16777619u;
                    h ^= (uint32_t) info.st_mtim.tv_nsec;
                    h *= 16777619u;
                }
            }
        }
    }

    if (h == last_font_key_hash) return 0;
    last_font_key_hash = h;

    LOG_INFO(mux_module, "Font context has changed");
    return 1;
}
