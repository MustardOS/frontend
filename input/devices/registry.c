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
    &tui_smpro_s_profile, &tui_spoon_profile,  &tui_brick_pro_profile, &tui_brick_profile,   &rgsp_profile,
    &rg28xx_h_profile,    &rg34xx_h_profile,   &rg34xx_sp_profile,     &rg35xx_2024_profile, &rg35xx_h_profile,
    &rg35xx_plus_profile, &rg35xx_pro_profile, &rg35xx_sp_profile,     &rg40xx_h_profile,    &rg40xx_v_profile,
    &rgcubexx_h_profile,  &gcs_h36s_profile,   &mgx_zero28_profile,    &rk_g350_v_profile,   &rk_pixel_2_profile,
    &rg_vita_pro_profile,
};

static const char board_name_path[] = "/opt/muos/device/config/board/name";

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
    FILE *board_name = fopen(board_name_path, "r");
    if (board_name) {
        char id[64];
        if (!fgets(id, sizeof(id), board_name)) {
            fprintf(stderr, "Unable to read board name from %s\n", board_name_path);
            fclose(board_name);
            return NULL;
        }

        int truncated = !strchr(id, '\n') && !feof(board_name);
        fclose(board_name);
        if (truncated) {
            fprintf(stderr, "Board name in %s is too long\n", board_name_path);
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
            fprintf(stderr, "Unsupported board '%s' in %s\n", start, board_name_path);
            return NULL;
        }
        return configured;
    }
    if (errno != ENOENT) {
        fprintf(stderr, "Unable to open %s: %s\n", board_name_path, strerror(errno));
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
