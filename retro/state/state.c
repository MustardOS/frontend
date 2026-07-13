#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "../core/muxretro.h"
#include "../core/runahead.h"
#include "../video/hw_render.h"

#define CORE_INFO_PATH OPT_SHARE_PATH "emulator/retroarch/info/"

static int saves_supported = 1;
void state_saves_init(const char *core_file_path) {
    saves_supported = 1;

    const char *base = strrchr(core_file_path, '/');
    base = base ? base + 1 : core_file_path;

    char core_name[128];
    snprintf(core_name, sizeof(core_name), "%s", base);
    char *ext = strstr(core_name, ".so");
    if (ext) *ext = '\0';

    char info_path[MAX_BUFFER_SIZE];
    snprintf(info_path, sizeof(info_path), "%s%s.info", CORE_INFO_PATH, core_name);

    FILE *f = fopen(info_path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        const char *key = str_trim(line);
        if (strcmp(key, "savestate_support") != 0) continue;

        char *val = str_trim(eq + 1);
        const size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        if (strcmp(val, "disabled") == 0) {
            saves_supported = 0;
            LOG_INFO(mux_module, "Save states disabled for this core (savestate_support=disabled in %s)", info_path);
        }
        break;
    }
    fclose(f);
}

int state_saves_supported(void) {
    return saves_supported;
}

int state_save(const char *path) {
    if (!saves_supported) return -1;
    if (!current_core.retro_serialize_size || !current_core.retro_serialize) return -1;

    hw_render_bridge_enter_core_call();

    size_t size = current_core.retro_serialize_size();
    if (size == 0) {
        hw_render_bridge_exit_core_call();
        return -1;
    }

    LOG_DEBUG(mux_module, "state_save: serialize_size=%zu", size);

    static size_t alloc_high_water = 0;
    size_t alloc = size + size / 4 + (1 << 20);
    if (alloc < alloc_high_water) alloc = alloc_high_water;
    alloc_high_water = alloc;

    void *buf = malloc(alloc);
    if (!buf) {
        hw_render_bridge_exit_core_call();
        return -1;
    }

    int ok = current_core.retro_serialize(buf, size);

    if (!ok) {
        const size_t regrown = current_core.retro_serialize_size();
        if (regrown > 0 && regrown != size && regrown <= alloc) {
            size = regrown;
            ok = current_core.retro_serialize(buf, size);
        }
    }

    hw_render_bridge_exit_core_call();

    if (!ok) {
        LOG_ERROR(mux_module, "Core failed to serialize state");
        free(buf);
        return -1;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to open '%s' for save state", path);
        free(buf);
        return -1;
    }

    const size_t written = fwrite(buf, 1, size, f);
    fclose(f);
    free(buf);

    if (written != size) {
        LOG_ERROR(mux_module, "Short write saving state to '%s'", path);
        return -1;
    }

    LOG_SUCCESS(mux_module, "Saved state to '%s' (%zu bytes)", path, size);
    return 0;
}

int state_load(const char *path) {
    if (!saves_supported) return -1;
    if (!current_core.retro_unserialize) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR(mux_module, "No save state found at '%s'", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return -1;
    }

    void *buf = malloc((size_t) size);
    if (!buf) {
        fclose(f);
        return -1;
    }

    const size_t got = fread(buf, 1, (size_t) size, f);
    fclose(f);

    if (got != (size_t) size) {
        LOG_ERROR(mux_module, "Short read loading state from '%s'", path);
        free(buf);
        return -1;
    }

    hw_render_bridge_enter_core_call();
    if (current_core.retro_serialize_size) current_core.retro_serialize_size();

    const int ok = current_core.retro_unserialize(buf, (size_t) size);
    hw_render_bridge_exit_core_call();

    free(buf);

    if (!ok) {
        LOG_ERROR(mux_module, "Core rejected save state from '%s'", path);
        return -1;
    }

    audio_bridge_clear_queued();
    runahead_invalidate();

    LOG_SUCCESS(mux_module, "Loaded state from '%s'", path);
    return 0;
}
