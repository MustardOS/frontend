#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <zlib.h>
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/ui/nav.h"
#include "../core/governor_boost.h"
#include "../core/perf.h"
#include "../core/muxretro.h"
#include "../core/runahead.h"
#include "../coreinfo/coreinfo.h"
#include "../cheevo/cheevo.h"
#include "../ui/ui_loading.h"
#include "core_state.h"
#include "state_format.h"
#include "state_writer.h"

static int saves_warmup_frames = 0;

static int write_pending = 0;
static int write_result = 0;

void state_saves_init(const char *core_file_path) {
    coreinfo_init(core_file_path);
    core_state_session_init();
    saves_warmup_frames = coreinfo_state_warmup_frames();
}

int state_saves_supported(void) {
    return core_state_available();
}

int state_saves_warmup_frames(void) {
    return saves_warmup_frames;
}

int state_save(const char *path) {
    if (!core_state_available() || !path) return -1;

    if (state_flush() != 0) LOG_WARN(mux_module, "The previous save-state write did not complete successfully");

    const uint64_t save_start = perf_begin();
    governor_boost_begin("state save");

    struct core_state_buffer state = {0};
    if (core_state_capture_prefixed(&state, STATE_HEADER_SIZE, 0, 0, 1, "save capture") != 0) {
        governor_boost_end();
        return -1;
    }
    const size_t core_size = state.size;

    size_t cheevo_size = cheevo_progress_size();
    if (cheevo_size > STATE_CHEEVO_LIMIT) cheevo_size = 0;

    if (core_size > SIZE_MAX - STATE_HEADER_SIZE || cheevo_size > SIZE_MAX - STATE_HEADER_SIZE - core_size) {
        core_state_buffer_release(&state);
        governor_boost_end();
        LOG_ERROR(mux_module, "Save-state allocation size overflow");
        return -1;
    }

    const size_t allocation_size = STATE_HEADER_SIZE + core_size + cheevo_size;
    uint8_t *buf = realloc(state.data, allocation_size);
    if (!buf) {
        core_state_buffer_release(&state);
        governor_boost_end();
        LOG_ERROR(mux_module, "Could not allocate %zu bytes for save state", allocation_size);
        return -1;
    }
    state.data = NULL;

    if (cheevo_size && cheevo_progress_save(buf + STATE_HEADER_SIZE + core_size, cheevo_size) != 0) cheevo_size = 0;

    memset(buf, 0, STATE_HEADER_SIZE);
    memcpy(buf, "PKST", 4);
    state_write_u16(buf + 4, STATE_VERSION);
    state_write_u16(buf + 6, cheevo_size ? STATE_FLAG_CHEEVO : 0);
    state_write_u32(buf + 8, (uint32_t) core_size);
    state_write_u32(buf + 12, (uint32_t) cheevo_size);
    state_write_u32(buf + 16, 0);
    state_write_u32(buf + 20, 0);

    const size_t output_size = STATE_HEADER_SIZE + core_size + cheevo_size;
    if (state_writer_submit(path, buf, output_size, core_size, cheevo_size) != 0) {
        free(buf);
        governor_boost_end();
        return -1;
    }

    write_pending = 1;
    write_result = 0;

    LOG_DEBUG(mux_module, "Queued state persistence for '%s'", path);
    governor_boost_end();
    perf_end(perf_stage_state_save, save_start);
    return 0;
}

int state_load(const char *path, const int show_message) {
    if (!core_state_available()) return -1;
    if (state_flush() != 0) {
        LOG_ERROR(mux_module, "Cannot load '%s' because its pending state write failed", path);
        return -1;
    }
    if (cheevo_hardcore_active()) {
        pause_menu_show_toast_timed("Save states cannot be loaded in Hardcore mode", tst_wait_s);
        return -1;
    }

    if (show_message) loading_message_show(lang.muxretro.content_resuming);
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

    const size_t state_limit = coreinfo_state_max_bytes();
    if (size <= 0 || (uint64_t) size > STATE_HEADER_SIZE + (uint64_t) state_limit + STATE_CHEEVO_LIMIT) {
        fclose(f);
        governor_boost_end();
        loading_message_hide();
        return -1;
    }

    uint8_t *buf = malloc((size_t) size);
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
            || declared_core > state_limit || declared_cheevo > STATE_CHEEVO_LIMIT || declared_total != (uint64_t) size
            || ((flags & STATE_FLAG_CHEEVO) == 0) != (declared_cheevo == 0)
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

    const int ok = core_state_restore(core_data, core_size, 0, "save restore") == 0;
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

int state_flush(void) {
    if (!write_pending) return 0;

    write_pending = 0;
    write_result = state_writer_flush();

    return write_result;
}

int state_write_poll(int *result) {
    if (write_pending) {
        if (state_writer_busy()) return 0;

        write_pending = 0;
        write_result = state_writer_flush();
    }

    if (result) *result = write_result;
    return 1;
}

void state_shutdown(void) {
    state_writer_shutdown();
}
