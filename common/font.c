#include <stdlib.h>

#include "font.h"
#include "common.h"
#include "log.h"
#include "options.h"
#include "config.h"
#include "device.h"
#include "../font/notosans_medium.h"
#include "../font/notosans_ar_medium.h"
#include "../font/notosans_jp_medium.h"
#include "../font/notosans_kr_medium.h"
#include "../font/notosans_sc_medium.h"
#include "../font/notosans_tc_medium.h"

int font_cache_count = 0;
char last_font_key[256] = "";

typedef struct {
    const char *lang;
    const void *font;
} font_lang_t;

typedef struct {
    char path[MAX_BUFFER_SIZE];
    lv_font_t *font;
} font_cache_t;

static font_cache_t font_cache[FONT_CACHE_MAX];

int get_font_size(void) {
    switch (device.MUX.WIDTH) {
        case 1024:
            return 32;
        case 1280:
            return 30;
        default:
            return 20;
    }
}

lv_font_t *get_language_font(void) {
    int font_size = get_font_size();
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

    return lv_tiny_ttf_create_data_ex(font_data, notosans_medium_ttf_len, font_size, cache_size);
}

void font_cache_clear(void) {
    for (int i = 0; i < font_cache_count; i++) lv_font_free(font_cache[i].font);
    font_cache_count = 0;
    LOG_SUCCESS(mux_module, "Font cache has been cleared");
}

static inline void apply_font(lv_obj_t *element, lv_font_t *font) {
    if (!element || !font) return;
    lv_obj_set_style_text_font(element, font, MU_OBJ_MAIN_DEFAULT);
}

static lv_font_t *load_font_from_file(const char *filepath) {
    char fs_path[MAX_BUFFER_SIZE];
    snprintf(fs_path, sizeof(fs_path), "M:%s", filepath);

    lv_font_t * font = lv_font_load(fs_path);
    if (!font) return NULL;

    font->fallback = get_language_font();
    return font;
}

static lv_font_t *load_font_cached(const char *path) {
    for (int i = 0; i < font_cache_count; i++) {
        if (strcmp(font_cache[i].path, path) == 0) {
            // LOG_SUCCESS(mux_module, "\tUsing font from cache");
            return font_cache[i].font;
        }
    }

    lv_font_t * font = load_font_from_file(path);
    // LOG_SUCCESS(mux_module, "\tUsing font from storage");

    if (!font) return NULL;

    if (font_cache_count < FONT_CACHE_MAX) {
        strncpy(font_cache[font_cache_count].path, path, MAX_BUFFER_SIZE - 1);
        font_cache[font_cache_count].path[MAX_BUFFER_SIZE - 1] = '\0';
        font_cache[font_cache_count].font = font;
        font_cache_count++;
    }

    return font;
}

void load_font_text(lv_obj_t *screen) {
    lv_font_t * language_font = get_language_font();

    // Always load the default font for the supporter credits module!
    if (strcasecmp(get_process_name(), "muxcredits") != 0 && config.SETTINGS.ADVANCED.FONT) {
        char theme_font_text_default[MAX_BUFFER_SIZE];
        char theme_font_text[MAX_BUFFER_SIZE];

        char *dimensions[15] = {mux_dimension, ""};
        for (int i = 0; i < 2; i++) {
            if ((snprintf(theme_font_text, sizeof(theme_font_text),
                            "%s/%sfont/%s/%s.bin", theme_base, dimensions[i],
                            config.SETTINGS.GENERAL.LANGUAGE, mux_module) >= 0 &&
                    file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text_default),
                            "%s/%sfont/%s/default.bin", theme_base, dimensions[i],
                            config.SETTINGS.GENERAL.LANGUAGE) >= 0 &&
                    file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text),
                            "%s/%sfont/%s.bin", theme_base, dimensions[i], mux_module) >= 0 &&
                    file_exist(theme_font_text)) ||

                (snprintf(theme_font_text, sizeof(theme_font_text_default),
                            "%s/%sfont/default.bin", theme_base, dimensions[i]) >= 0 &&
                    file_exist(theme_font_text))) {

                LOG_INFO(mux_module, "Loading Main Theme Font: %s", theme_font_text);

                lv_font_t * font = load_font_cached(theme_font_text);
                if (font) apply_font(screen, font);

                return;
            }
        }
    }

    LOG_INFO(mux_module, "Loading Default Language Font");
    lv_obj_set_style_text_font(screen, language_font, MU_OBJ_MAIN_DEFAULT);
}

void load_font_section(const char *section, lv_obj_t *element) {
    if (config.SETTINGS.ADVANCED.FONT) {
        char theme_font_section[MAX_BUFFER_SIZE];

        char *dimensions[15] = {mux_dimension, ""};
        for (int i = 0; i < 2; i++) {
            if ((snprintf(theme_font_section, sizeof(theme_font_section),
                            "%s/%sfont/%s/%s/%s.bin", theme_base, dimensions[i],
                            config.SETTINGS.GENERAL.LANGUAGE, section, mux_module) >= 0 &&
                    file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                            "%s/%sfont/%s/%s/default.bin", theme_base, dimensions[i],
                            config.SETTINGS.GENERAL.LANGUAGE, section) >= 0 &&
                    file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                            "%s/%sfont/%s/%s.bin", theme_base, dimensions[i], section, mux_module) >= 0 &&
                    file_exist(theme_font_section)) ||

                (snprintf(theme_font_section, sizeof(theme_font_section),
                            "%s/%sfont/%s/default.bin", theme_base, dimensions[i], section) >= 0 &&
                    file_exist(theme_font_section))) {

                LOG_INFO(mux_module, "Loading Section '%s' Font: %s", section, theme_font_section);

                lv_font_t * font = load_font_cached(theme_font_section);
                if (font) apply_font(element, font);

                return;
            }
        }
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
