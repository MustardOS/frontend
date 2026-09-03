#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/governor_boost.h"
#include "../core/muxretro.h"
#include "../core/runahead.h"
#include "../link/link.h"
#include "../netplay/netplay.h"
#include "../settings/settings.h"
#include "../video/image_writer.h"
#include "core_state.h"
#include "gamestate.h"
#include "history.h"

#define HISTORY_BUDGET_LIMIT (128U * 1024U * 1024U)

struct history_entry {
    struct core_state_buffer buffer;
    enum history_source source;
    long long created;
    int slot;
    int has_thumbnail;
};

static char base_dir[512];

static struct history_entry entries[HISTORY_DEPTH_MAX];
static int entry_count = 0;

static int ring_slots = 0;

static size_t cached_entry_size = 0;
static int budget_announced = 0;

static int thumbnail_path(const int slot, char *out, const size_t size) {
    if (!base_dir[0] || slot < 0) return 0;
    return snprintf(out, size, "%s/history_%d.png", base_dir, slot) > 0;
}

void history_set_directory(const char *state_dir) {
    snprintf(base_dir, sizeof(base_dir), "%s", state_dir ? state_dir : "");

    for (int i = 0; i < HISTORY_DEPTH_MAX; i++)
        entries[i].slot = i;

    for (int i = 0; i < HISTORY_DEPTH_MAX; i++) {
        char path[512];
        if (thumbnail_path(i, path, sizeof(path))) remove(path);
    }
}

const char *history_thumbnail(const int index) {
    if (index < 0 || index >= history_count()) return NULL;

    const struct history_entry *entry = &entries[entry_count - 1 - index];
    if (!entry->has_thumbnail) return NULL;

    static char path[512];
    return thumbnail_path(entry->slot, path, sizeof(path)) ? path : NULL;
}

static int history_allowed(void) {
    if (session_settings.history_depth <= 0) return 0;
    if (!state_saves_supported()) return 0;
    if (netplay_is_active()) return 0;
    if (link_local_active()) return 0;
    return 1;
}

static size_t history_budget_bytes(void) {
    static size_t budget = 0;
    if (budget) return budget;

    budget = HISTORY_BUDGET_LIMIT;

    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            unsigned long total = 0;
            if (sscanf(line, "MemTotal: %lu kB", &total) == 1 && total > 0) {
                const size_t share = (size_t) total * 1024 / 8;
                if (share < budget) budget = share;
                break;
            }
        }
        fclose(f);
    }

    return budget;
}

int history_effective_depth(void) {
    if (!history_allowed()) return 0;

    int wanted = session_settings.history_depth;
    if (wanted < 1) return 0;
    if (wanted > HISTORY_DEPTH_MAX) wanted = HISTORY_DEPTH_MAX;

    if (!cached_entry_size) cached_entry_size = core_state_reported_size(0, "history size");
    if (!cached_entry_size) return 0;

    const size_t per_slot = cached_entry_size + cached_entry_size / 8;
    const size_t affordable = history_budget_bytes() / per_slot;

    if (affordable < 1) {
        if (!budget_announced) {
            budget_announced = 1;
            LOG_INFO(
                mux_module, "Save state history is unavailable because one state needs %zu bytes of a %zu byte budget",
                cached_entry_size, history_budget_bytes()
            );
        }
        return 0;
    }

    return wanted < (int) affordable ? wanted : (int) affordable;
}

static void release_slot(const int index) {
    core_state_buffer_release(&entries[index].buffer);
    entries[index].created = 0;

    if (entries[index].has_thumbnail) {
        char path[512];
        if (thumbnail_path(entries[index].slot, path, sizeof(path))) remove(path);
        entries[index].has_thumbnail = 0;
    }
}

static void drop_oldest(void) {
    if (entry_count <= 0) return;

    if (entries[0].has_thumbnail) {
        char path[512];
        if (thumbnail_path(entries[0].slot, path, sizeof(path))) remove(path);
        entries[0].has_thumbnail = 0;
    }

    const struct history_entry oldest = entries[0];
    memmove(&entries[0], &entries[1], sizeof(entries[0]) * (size_t) (entry_count - 1));
    entries[entry_count - 1] = oldest;
    entries[entry_count - 1].created = 0;

    entry_count--;
}

void history_push(const enum history_source source) {
    cached_entry_size = 0;

    const int depth = history_effective_depth();
    if (depth <= 0) {
        history_clear();
        return;
    }

    if (depth != ring_slots) {
        history_clear();
        ring_slots = depth;
    }

    while (entry_count >= depth)
        drop_oldest();

    if (core_state_capture(&entries[entry_count].buffer, 0, 0, 0, "history capture") != 0) {
        LOG_WARN(mux_module, "Could not keep the current moment before loading a save state");
        return;
    }

    entries[entry_count].source = source;
    entries[entry_count].created = (long long) time(NULL);

    char path[512];
    entries[entry_count].has_thumbnail = 0;

    if (thumbnail_path(entries[entry_count].slot, path, sizeof(path))) {
        if (pause_menu_is_active()) {
            const char *pending = gamestate_pending_thumbnail();

            image_writer_flush();
            entries[entry_count].has_thumbnail = pending && copy_file(pending, path) == 0;
        } else {
            entries[entry_count].has_thumbnail = pause_menu_store_clean_screenshot(path, 1) == 0;
        }
    }

    entry_count++;

    LOG_DEBUG(mux_module, "Kept a save state history moment, holding %d of %d", entry_count, depth);
}

int history_count(void) {
    return history_allowed() ? entry_count : 0;
}

int history_describe(const int index, enum history_source *source, long long *created) {
    if (index < 0 || index >= history_count()) return -1;

    const struct history_entry *entry = &entries[entry_count - 1 - index];

    if (source) *source = entry->source;
    if (created) *created = entry->created;

    return 0;
}

int history_restore(const int index) {
    if (index < 0 || index >= history_count()) return -1;

    const struct history_entry *entry = &entries[entry_count - 1 - index];

    governor_boost_begin("history restore");

    if (core_state_restore(entry->buffer.data, entry->buffer.size, 0, "history restore") != 0) {
        governor_boost_end();
        LOG_ERROR(mux_module, "The core refused a stored history moment");
        return -1;
    }

    audio_bridge_clear_queued();
    runahead_invalidate();

    governor_boost_end();

    LOG_INFO(mux_module, "Restored save state history moment %d of %d", index + 1, entry_count);
    return 0;
}

void history_clear(void) {
    for (int i = 0; i < HISTORY_DEPTH_MAX; i++)
        release_slot(i);

    entry_count = 0;
    ring_slots = 0;
}

void history_shutdown(void) {
    history_clear();
    cached_entry_size = 0;
    budget_announced = 0;
}
