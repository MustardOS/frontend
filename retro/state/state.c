#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "../core/governor_boost.h"
#include "../core/muxretro.h"
#include "../core/runahead.h"
#include "../video/hw_render.h"
#include "../ui/ui_loading.h"

#define CORE_INFO_PATH OPT_SHARE_PATH "emulator/retroarch/info/"

static int atomic_write_state(const char *path, const void *data, const size_t size) {
    char tmp_path[MAX_BUFFER_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to open '%s' for save state", tmp_path);
        return -1;
    }

    const int ok = fwrite(data, 1, size, f) == size;
    if (ok) {
        fflush(f);
        fdatasync(fileno(f));
    }
    fclose(f);

    if (!ok) {
        LOG_ERROR(mux_module, "Short write saving state to '%s'", tmp_path);
        remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) != 0) {
        LOG_ERROR(mux_module, "Failed to rename '%s' to '%s'", tmp_path, path);
        return -1;
    }

    char dir_path[MAX_BUFFER_SIZE];
    snprintf(dir_path, sizeof(dir_path), "%s", path);
    char *slash = strrchr(dir_path, '/');
    if (slash) *slash = '\0';

    const int dir_fd = open(dir_path, O_RDONLY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    return 0;
}

static int saves_supported = 1;
static int saves_warmup_frames = 0;
void state_saves_init(const char *core_file_path) {
    saves_supported = 1;
    saves_warmup_frames = 0;

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
        char *val = str_trim(eq + 1);
        const size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        if (strcmp(key, "savestate_support") == 0 && strcmp(val, "disabled") == 0) {
            saves_supported = 0;
            LOG_INFO(mux_module, "Save states disabled for this core (savestate_support=disabled in %s)", info_path);
        } else if (strcmp(key, "savestate_warmup_frames") == 0) {
            char *end = NULL;
            const long frames = strtol(val, &end, 10);
            if (end != val && frames >= 0 && frames <= 600) saves_warmup_frames = (int) frames;
        }
    }
    fclose(f);
}

int state_saves_supported(void) {
    return saves_supported;
}

int state_saves_warmup_frames(void) {
    return saves_warmup_frames;
}

int state_save(const char *path) {
    if (!saves_supported) return -1;
    if (!current_core.retro_serialize_size || !current_core.retro_serialize) return -1;

    governor_boost_begin("state save");

    hw_render_bridge_enter_core_call();

    size_t size = current_core.retro_serialize_size();
    if (size == 0) {
        hw_render_bridge_exit_core_call();
        governor_boost_end();
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
        governor_boost_end();
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
        LOG_ERROR(mux_module, "Core failed to serialise state");
        free(buf);
        governor_boost_end();
        return -1;
    }

    const int ok_write = atomic_write_state(path, buf, size) == 0;
    free(buf);

    if (!ok_write) {
        governor_boost_end();
        return -1;
    }

    LOG_SUCCESS(mux_module, "Saved state to '%s' (%zu bytes)", path, size);
    governor_boost_end();
    return 0;
}

int state_load(const char *path) {
    if (!saves_supported) return -1;
    if (!current_core.retro_unserialize) return -1;

    loading_message_show(lang.muxretro.content_resuming);
    governor_boost_begin("state load");

    const uint64_t load_start = SDL_GetPerformanceCounter();
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_ERROR(mux_module, "No save state found at '%s'", path);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    void *buf = malloc((size_t) size);
    if (!buf) {
        fclose(f);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    const size_t got = fread(buf, 1, (size_t) size, f);
    fclose(f);
    const uint64_t read_done = SDL_GetPerformanceCounter();

    if (got != (size_t) size) {
        LOG_ERROR(mux_module, "Short read loading state from '%s'", path);
        free(buf);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    hw_render_bridge_enter_core_call();
    if (current_core.retro_serialize_size) current_core.retro_serialize_size();

    const int ok = current_core.retro_unserialize(buf, (size_t) size);
    hw_render_bridge_exit_core_call();
    const uint64_t deserialise_done = SDL_GetPerformanceCounter();

    free(buf);

    if (!ok) {
        LOG_ERROR(mux_module, "Core rejected save state from '%s'", path);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    audio_bridge_clear_queued();
    runahead_invalidate();

    const double to_ms = 1000.0 / (double) SDL_GetPerformanceFrequency();
    LOG_INFO(
        mux_module, "State restore timing: read %.2f ms, deserialise %.2f ms, total %.2f ms (%ld bytes)",
        (double) (read_done - load_start) * to_ms, (double) (deserialise_done - read_done) * to_ms,
        (double) (deserialise_done - load_start) * to_ms, size
    );

    LOG_SUCCESS(mux_module, "Loaded state from '%s'", path);
    governor_boost_end();
    loading_message_hide();
    return 0;
}
