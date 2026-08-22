#include <stdio.h>
#include <stdlib.h>
#include "theme_base.h"
#include "options.h"
#include "config.h"
#include "init.h"
#include "fileio.h"
#include "strutil.h"
#include "log.h"

const char *theme_base;
char *theme_back_compat[theme_compat];

static void compat_theme_init(void) {
    theme_back_compat[0] = config.system.version;
    theme_back_compat[1] = "2601.1_FUNKY_JACARANDA";
    theme_back_compat[2] = "2601.0_JACARANDA";
    theme_back_compat[3] = "2508.4_LOOSE_GOOSE";
    theme_back_compat[4] = "2508.3_GOLDEN_GOOSE";
    theme_back_compat[5] = "2508.2_SILLY_GOOSE";
    theme_back_compat[6] = "2508.1_CANADA_GOOSE";
    theme_back_compat[7] = "2508.0_GOOSE";
}

const char *get_theme_base(void) {
    compat_theme_init();

    static char storage_theme[MAX_BUFFER_SIZE];
    char theme_version_file[MAX_BUFFER_SIZE];

    const char *paths[2];
    int path_count = 0;

    if (!config.boot.factory_reset) {
        const int written =
            snprintf(storage_theme, sizeof(storage_theme), RUN_STORAGE_PATH "theme/%s", config.theme.active);
        if (written > 0 && (size_t) written < sizeof(storage_theme)) paths[path_count++] = storage_theme;
    }

    paths[path_count++] = INTERNAL_THEME;

    for (int p = 0; p < path_count; p++) {
        snprintf(theme_version_file, sizeof(theme_version_file), "%s/version.txt", paths[p]);
        if (!file_exist(theme_version_file)) continue;

        char *theme_version = read_line_char_from(theme_version_file, 1);
        if (!theme_version || !*theme_version) {
            free(theme_version);
            continue;
        }

        int compatible = 0;
        for (size_t i = 0; i < theme_compat; i++) {
            const char *compat = theme_back_compat[i];
            if (!compat) continue;
            if (str_startswith(compat, theme_version)) {
                compatible = 1;
                break;
            }
        }

        if (compatible) {
            free(theme_version);
            return paths[p];
        }

        LOG_WARN(mux_module, "Incompatible Theme Detected (%s): %s", paths[p], theme_version);
        free(theme_version);
        break;
    }

    LOG_WARN(mux_module, "Missing or incompatible theme version, falling back to internal");
    return INTERNAL_THEME;
}
