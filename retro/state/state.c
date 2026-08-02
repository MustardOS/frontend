#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <zlib.h>
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../../common/ui/nav.h"
#include "../core/core.h"
#include "../core/governor_boost.h"
#include "../core/perf.h"
#include "../core/muxretro.h"
#include "../core/runahead.h"
#include "../cheevo/cheevo.h"
#include "../video/hw_render.h"
#include "../ui/ui_loading.h"

#define CORE_INFO_PATH     OPT_SHARE_PATH "emulator/retroarch/info/"
#define STATE_HEADER_SIZE  32U
#define STATE_VERSION      1U
#define STATE_FLAG_CHEEVO  1U
#define STATE_CORE_LIMIT   (512U * 1024U * 1024U)
#define STATE_CHEEVO_LIMIT (16U * 1024U * 1024U)

static void state_write_u16(uint8_t *data, const uint16_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
}

static void state_write_u32(uint8_t *data, const uint32_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}

static uint16_t state_read_u16(const uint8_t *data) {
    return (uint16_t) data[0] | (uint16_t) data[1] << 8;
}

static uint32_t state_read_u32(const uint8_t *data) {
    return (uint32_t) data[0] | (uint32_t) data[1] << 8 | (uint32_t) data[2] << 16 | (uint32_t) data[3] << 24;
}

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

    const uint64_t save_start = perf_begin();
    governor_boost_begin("state save");

    hw_render_bridge_enter_core_call();

    size_t core_size = current_core.retro_serialize_size();
    if (core_size == 0 || core_size > STATE_CORE_LIMIT) {
        hw_render_bridge_exit_core_call();
        governor_boost_end();
        return -1;
    }

    LOG_DEBUG(mux_module, "state_save: serialise_size=%zu", core_size);

    static size_t alloc_high_water = 0;
    size_t alloc = core_size + core_size / 4 + (1 << 20);
    if (alloc < alloc_high_water) alloc = alloc_high_water;
    alloc_high_water = alloc;

    size_t cheevo_size = cheevo_progress_size();
    if (cheevo_size > STATE_CHEEVO_LIMIT) cheevo_size = 0;

    uint8_t *buf = malloc(STATE_HEADER_SIZE + alloc + cheevo_size);
    if (!buf) {
        hw_render_bridge_exit_core_call();
        governor_boost_end();
        return -1;
    }

    int ok = current_core.retro_serialize(buf + STATE_HEADER_SIZE, core_size);

    if (!ok) {
        const size_t regrown = current_core.retro_serialize_size();
        if (regrown > 0 && regrown != core_size && regrown <= alloc) {
            core_size = regrown;
            ok = current_core.retro_serialize(buf + STATE_HEADER_SIZE, core_size);
        }
    }

    hw_render_bridge_exit_core_call();

    if (!ok) {
        LOG_ERROR(mux_module, "Core failed to serialise state");
        free(buf);
        governor_boost_end();
        return -1;
    }

    if (cheevo_size && cheevo_progress_save(buf + STATE_HEADER_SIZE + core_size, cheevo_size) != 0) cheevo_size = 0;

    memset(buf, 0, STATE_HEADER_SIZE);
    memcpy(buf, "PKST", 4);
    state_write_u16(buf + 4, STATE_VERSION);
    state_write_u16(buf + 6, cheevo_size ? STATE_FLAG_CHEEVO : 0);
    state_write_u32(buf + 8, (uint32_t) core_size);
    state_write_u32(buf + 12, (uint32_t) cheevo_size);
    state_write_u32(buf + 16, crc32(0, buf + STATE_HEADER_SIZE, (uInt) core_size));
    state_write_u32(buf + 20, cheevo_size ? crc32(0, buf + STATE_HEADER_SIZE + core_size, (uInt) cheevo_size) : 0);

    const size_t output_size = STATE_HEADER_SIZE + core_size + cheevo_size;
    const int ok_write = atomic_write_state(path, buf, output_size) == 0;
    free(buf);

    if (!ok_write) {
        governor_boost_end();
        return -1;
    }

    LOG_SUCCESS(
        mux_module, "Saved state to '%s' (%zu core bytes, %zu achievement bytes)", path, core_size, cheevo_size
    );
    governor_boost_end();
    perf_end(perf_stage_state_save, save_start);
    return 0;
}

int state_load(const char *path) {
    if (!saves_supported) return -1;
    if (!current_core.retro_unserialize) return -1;
    if (cheevo_hardcore_active()) {
        pause_menu_show_toast_timed("Save states cannot be loaded in Hardcore mode", tst_wait_s);
        return -1;
    }

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

    if (size <= 0 || (uint64_t) size > STATE_HEADER_SIZE + STATE_CORE_LIMIT + STATE_CHEEVO_LIMIT) {
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

    const uint8_t *core_data = buf;
    size_t core_size = (size_t) size;
    const uint8_t *cheevo_data = NULL;
    size_t cheevo_size = 0;
    int legacy = 1;

    if ((size_t) size >= STATE_HEADER_SIZE && memcmp(buf, "PKST", 4) == 0) {
        const uint16_t version = state_read_u16(buf + 4);
        const uint16_t flags = state_read_u16(buf + 6);
        const uint32_t declared_core = state_read_u32(buf + 8);
        const uint32_t declared_cheevo = state_read_u32(buf + 12);
        const uint64_t declared_total = STATE_HEADER_SIZE + (uint64_t) declared_core + declared_cheevo;

        if (version != STATE_VERSION || (flags & ~STATE_FLAG_CHEEVO) != 0 || declared_core == 0
            || declared_core > STATE_CORE_LIMIT || declared_cheevo > STATE_CHEEVO_LIMIT
            || declared_total != (uint64_t) size || ((flags & STATE_FLAG_CHEEVO) == 0) != (declared_cheevo == 0)
            || state_read_u32(buf + 16) != crc32(0, buf + STATE_HEADER_SIZE, declared_core)
            || (declared_cheevo
                && state_read_u32(buf + 20) != crc32(0, buf + STATE_HEADER_SIZE + declared_core, declared_cheevo))) {
            LOG_ERROR(mux_module, "Invalid Pickles save-state envelope in '%s'", path);
            free(buf);
            governor_boost_end();
            loading_message_hide();
            return -1;
        }

        legacy = 0;
        core_data = buf + STATE_HEADER_SIZE;
        core_size = declared_core;
        cheevo_data = declared_cheevo ? core_data + declared_core : NULL;
        cheevo_size = declared_cheevo;
    }

    hw_render_bridge_enter_core_call();
    if (current_core.retro_serialize_size) current_core.retro_serialize_size();

    const int ok = current_core.retro_unserialize(core_data, core_size);
    hw_render_bridge_exit_core_call();
    const uint64_t deserialise_done = SDL_GetPerformanceCounter();

    if (!ok) {
        free(buf);
        LOG_ERROR(mux_module, "Core rejected save state from '%s'", path);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    audio_bridge_clear_queued();
    runahead_invalidate();

    if (cheevo_data) {
        if (cheevo_progress_load(cheevo_data, cheevo_size) != 0)
            LOG_WARN(mux_module, "Achievement progress could not be restored from '%s'", path);
    } else {
        cheevo_progress_reset();
        if (legacy) LOG_INFO(mux_module, "Achievement progress reset after loading a legacy save state");
    }

    free(buf);

    const double to_ms = 1000.0 / (double) SDL_GetPerformanceFrequency();
    LOG_INFO(
        mux_module, "State restore timing: read %.2f ms, deserialise %.2f ms, total %.2f ms (%ld bytes)",
        (double) (read_done - load_start) * to_ms, (double) (deserialise_done - read_done) * to_ms,
        (double) (deserialise_done - load_start) * to_ms, (long) core_size
    );

    LOG_SUCCESS(mux_module, "Loaded state from '%s'", path);
    governor_boost_end();
    loading_message_hide();
    return 0;
}
