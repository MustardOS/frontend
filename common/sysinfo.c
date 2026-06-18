#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "init.h"
#include "log.h"
#include "language.h"
#include "device.h"
#include "config.h"
#include "fileio.h"
#include "strutil.h"

int is_network_connected(void) {
    if (file_exist(device.NETWORK.STATE)) {
        if (strcasecmp("up", read_all_char_from(device.NETWORK.STATE)) == 0) return 1;
    }

    return 0;
}

int is_bluetooth_connected(void) {
    FILE *paired = fopen(CONF_CONFIG_PATH "bluetooth/paired", "r");
    if (!paired) return 0;

    char line[160];
    int connected = 0;

    while (fgets(line, sizeof(line), paired)) {
        char *space = strchr(line, ' ');
        if (space && strtol(space + 1, NULL, 10) == 1) {
            connected = 1;
            break;
        }
    }

    fclose(paired);
    return connected;
}

int resolution_check(const char *theme_path) {
    LOG_INFO(mux_module, "Inspecting theme for supported resolutions: %s", theme_path);
    const char *resolutions[] = {"640x480", "720x480", "720x576", "720x720", "1024x768", "1280x720", "1920x1080"};

    // Check if the folder name matches any target resolutions
    for (size_t j = 0; j < A_SIZE(resolutions); j++) {
        char theme_resolution_path[MAX_BUFFER_SIZE];
        snprintf(theme_resolution_path, sizeof(theme_resolution_path), "%s/%s", theme_path, resolutions[j]);
        if (dir_exist(theme_resolution_path)) {
            LOG_SUCCESS(mux_module, "Found supported resolution: %s", resolutions[j]);
            return 1;
        }
    }

    LOG_WARN(mux_module, "No supported resolutions found");

    return 0;
}

struct screen_dimension get_device_dimensions(void) {
    struct screen_dimension dims;
    if (read_line_int_from(device.SCREEN.HDMI, 1)) {
        dims.WIDTH = device.SCREEN.EXTERNAL.WIDTH;
        dims.HEIGHT = device.SCREEN.EXTERNAL.HEIGHT;
    } else {
        dims.WIDTH = device.SCREEN.INTERNAL.WIDTH;
        dims.HEIGHT = device.SCREEN.INTERNAL.HEIGHT;
    }

    LOG_INFO(mux_module, "Screen Output dims: %dx%d", dims.WIDTH, dims.HEIGHT);
    return dims;
}

int brightness_to_percent(int val) {
    if (device.SCREEN.BRIGHT <= 0) return 0;
    return (val * 100) / device.SCREEN.BRIGHT;
}

int volume_to_percent(int val) {
    int max = config.SETTINGS.ADVANCED.OVERDRIVE ? 200 : 100;
    return (val * 100) / max;
}

char *get_version(int verify) {
    static char version[64];
    snprintf(version, sizeof(version), "%s%s", str_replace(config.SYSTEM.VERSION, "_", " "), verify ? "*" : "");
    return version;
}

char *get_build(void) {
    static char build[16];
    snprintf(build, sizeof(build), "%s", config.SYSTEM.BUILD);
    return build;
}

char *get_storage_label(const char *path) {
    if (!path) return "Unknown";

    if (strncmp(path, device.STORAGE.ROM.MOUNT, strlen(device.STORAGE.ROM.MOUNT)) == 0) return lang.MUXSPACE.PRIMARY;
    if (strncmp(path, device.STORAGE.SDCARD.MOUNT, strlen(device.STORAGE.SDCARD.MOUNT)) == 0) return lang.MUXSPACE.SECONDARY;
    if (strncmp(path, device.STORAGE.USB.MOUNT, strlen(device.STORAGE.USB.MOUNT)) == 0) return lang.MUXSPACE.EXTERNAL;

    return "Unknown";
}

const char *resolve_info_path(const char *rel) {
    if (rel[0] == '/') return NULL;

    static char path[PATH_MAX];

    struct {
        const char *base;
        const char *sub;
    } sources[] = {
            {device.STORAGE.USB.MOUNT,    "MUOS/info"},
            {device.STORAGE.SDCARD.MOUNT, "MUOS/info"},
            {device.STORAGE.ROM.MOUNT,    "MUOS/info"},
            {"/opt/muos/share",           "info"}
    };

    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        if (!sources[i].base || !*sources[i].base) continue;

        snprintf(path, sizeof(path), "%s/%s/%s",
                 sources[i].base, sources[i].sub, rel);

        remove_double_slashes(path);
        if (file_exist(path)) return path;
    }

    return NULL;
}
