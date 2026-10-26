#include <stdio.h>
#include <stdlib.h>
#include <common/display/theme_base.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/storage/fileio.h>
#include <common/base/strutil.h>
#include <common/runtime/log.h>

const char *theme_base;

static const char *const theme_back_compat[] = {
    "2606.0_ANDROMEDA",   "2601.2_MIDNIGHT_JACARANDA", "2601.1_FUNKY_JACARANDA",
    "2601.0_JACARANDA",   "2508.4_LOOSE_GOOSE",        "2508.3_GOLDEN_GOOSE",
    "2508.2_SILLY_GOOSE", "2508.1_CANADA_GOOSE",       "2508.0_GOOSE",
};

int theme_version_compatible(const char *theme_version) {
    if (!theme_version || !*theme_version) return 0;
    if (str_startswith(config.system.version, theme_version)) return 1;

    for (size_t i = 0; i < sizeof(theme_back_compat) / sizeof(theme_back_compat[0]); i++) {
        if (str_startswith(theme_back_compat[i], theme_version)) return 1;
    }

    return 0;
}

const char *get_theme_base(void) {
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

        if (theme_version_compatible(theme_version)) {
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
