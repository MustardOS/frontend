#include <stdio.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/log.h"
#include "core.h"
#include "muxretro.h"

int state_save(const char *path) {
    if (!current_core.retro_serialize_size || !current_core.retro_serialize) return -1;

    const size_t size = current_core.retro_serialize_size();
    if (size == 0) return -1;

    void *buf = malloc(size);
    if (!buf) return -1;

    if (!current_core.retro_serialize(buf, size)) {
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

    const int ok = current_core.retro_unserialize(buf, (size_t) size);
    free(buf);

    if (!ok) {
        LOG_ERROR(mux_module, "Core rejected save state from '%s'", path);
        return -1;
    }

    LOG_SUCCESS(mux_module, "Loaded state from '%s'", path);
    return 0;
}
