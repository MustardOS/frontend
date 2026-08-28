#include "registry.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "tui-brick/brick.h"
#include "tui-brick-pro/brick_pro.h"
#include "h700/h700.h"
#include "tui-spoon/smart_pro.h"
#include "tui-smpro-s/smart_pro_s.h"
#include "evdev/portable.h"

static const struct device_backend *const BACKENDS[] = {
    &TUI_SMPRO_S_PROFILE, &TUI_SPOON_PROFILE,  &TUI_BRICK_PRO_PROFILE, &TUI_BRICK_PROFILE,   &RGSP_PROFILE,
    &RG28XX_H_PROFILE,    &RG34XX_H_PROFILE,   &RG34XX_SP_PROFILE,     &RG35XX_2024_PROFILE, &RG35XX_H_PROFILE,
    &RG35XX_PLUS_PROFILE, &RG35XX_PRO_PROFILE, &RG35XX_SP_PROFILE,     &RG40XX_H_PROFILE,    &RG40XX_V_PROFILE,
    &RGCUBEXX_H_PROFILE,  &GCS_H36S_PROFILE,   &MGX_ZERO28_PROFILE,    &RK_G350_V_PROFILE,   &RK_PIXEL_2_PROFILE,
    &RG_VITA_PRO_PROFILE,
};

static const char BOARD_NAME_PATH[] = "/opt/muos/device/config/board/name";

const struct device_backend *const *device_registry_all(size_t *count) {
    if (count) {
        *count = sizeof(BACKENDS) / sizeof(BACKENDS[0]);
    }
    return BACKENDS;
}

const struct device_backend *device_registry_find(const char *id) {
    if (!id || !*id) {
        return NULL;
    }

    size_t count = 0;
    const struct device_backend *const *backends = device_registry_all(&count);
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(id, backends[i]->id) == 0) {
            return backends[i];
        }
    }
    return NULL;
}

const struct device_backend *device_registry_detect(void) {
    FILE *board_name = fopen(BOARD_NAME_PATH, "r");
    if (board_name) {
        char id[64];
        if (!fgets(id, sizeof(id), board_name)) {
            fprintf(stderr, "Unable to read board name from %s\n", BOARD_NAME_PATH);
            fclose(board_name);
            return NULL;
        }

        int truncated = !strchr(id, '\n') && !feof(board_name);
        fclose(board_name);
        if (truncated) {
            fprintf(stderr, "Board name in %s is too long\n", BOARD_NAME_PATH);
            return NULL;
        }

        char *start = id;
        while (*start && isspace((unsigned char) *start))
            ++start;
        char *end = start + strlen(start);
        while (end > start && isspace((unsigned char) end[-1]))
            --end;
        *end = '\0';

        const struct device_backend *configured = device_registry_find(start);
        if (!configured) {
            fprintf(stderr, "Unsupported board '%s' in %s\n", start, BOARD_NAME_PATH);
            return NULL;
        }
        return configured;
    }
    if (errno != ENOENT) {
        fprintf(stderr, "Unable to open %s: %s\n", BOARD_NAME_PATH, strerror(errno));
        return NULL;
    }

    const struct device_backend *best = NULL;
    size_t count = 0;
    const struct device_backend *const *backends = device_registry_all(&count);

    for (size_t i = 0; i < count; ++i) {
        const struct device_backend *candidate = backends[i];
        if (candidate->probe && candidate->probe() && (!best || candidate->probe_priority > best->probe_priority)) {
            best = candidate;
        }
    }
    return best;
}

void device_registry_print(FILE *stream) {
    size_t count = 0;
    const struct device_backend *const *backends = device_registry_all(&count);
    for (size_t i = 0; i < count; ++i) {
        fprintf(stream, "%s\t%s\n", backends[i]->id, backends[i]->name);
    }
}
