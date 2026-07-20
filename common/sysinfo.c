#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "init.h"
#include "log.h"
#include "device.h"
#include "config.h"
#include "fileio.h"
#include "strutil.h"

int is_network_connected(void) {
    if (file_exist(device.network.state)) {
        char *state = read_all_char_from(device.network.state);
        const int up = strcasecmp("up", state) == 0;
        free(state);

        if (up) return 1;
    }

    return 0;
}

int is_bluetooth_connected(void) {
    FILE *paired = fopen(CONF_CONFIG_PATH "bluetooth/paired", "r");
    if (!paired) return 0;

    char line[160];
    int connected = 0;

    while (fgets(line, sizeof(line), paired)) {
        const char *space = strchr(line, ' ');
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
    if (read_line_int_from(device.screen.hdmi, 1)) {
        dims.width = device.screen.external.width;
        dims.height = device.screen.external.height;
    } else {
        dims.width = device.screen.internal.width;
        dims.height = device.screen.internal.height;
    }

    LOG_INFO(mux_module, "Screen Output dims: %dx%d", dims.width, dims.height);
    return dims;
}

int brightness_to_percent(const int val) {
    if (device.screen.bright <= 0) return 0;
    return val * 100 / device.screen.bright;
}

int volume_to_percent(const int val) {
    const int max = config.settings.advanced.overdrive ? 200 : 100;
    return val * 100 / max;
}

char *get_version(const int verify) {
    static char version[64];
    char *display_version = str_replace(config.system.version, "_", " ");
    snprintf(version, sizeof(version), "%s%s", display_version, verify ? "*" : "");
    free(display_version);
    return version;
}

char *get_build(void) {
    static char build[16];
    snprintf(build, sizeof(build), "%s", config.system.build);
    return build;
}

char *get_storage_label(const char *path, const char *primary, const char *secondary, const char *external) {
    if (!path) return "Unknown";

    if (strncmp(path, device.storage.rom.mount, strlen(device.storage.rom.mount)) == 0) return (char *) primary;
    if (strncmp(path, device.storage.sdcard.mount, strlen(device.storage.sdcard.mount)) == 0) return (char *) secondary;
    if (strncmp(path, device.storage.usb.mount, strlen(device.storage.usb.mount)) == 0) return (char *) external;

    return "Unknown";
}

const char *resolve_info_path(const char *rel) {
    if (rel[0] == '/') return NULL;

    static char path[PATH_MAX];

    const struct {
        const char *base;
        const char *sub;
    } sources[] = {
        {device.storage.usb.mount, "MUOS/info"},
        {device.storage.sdcard.mount, "MUOS/info"},
        {device.storage.rom.mount, "MUOS/info"},
        {"/opt/muos/share", "info"}
    };

    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        if (!sources[i].base || !*sources[i].base) continue;

        snprintf(path, sizeof(path), "%s/%s/%s", sources[i].base, sources[i].sub, rel);

        remove_double_slashes(path);
        if (file_exist(path)) return path;
    }

    return NULL;
}
