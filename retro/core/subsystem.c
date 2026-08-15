#include <stdio.h>
#include <string.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/options.h"
#include "../../common/log.h"
#include "subsystem.h"

struct subsystem_entry subsystem_list[SUBSYSTEM_MAX];
int subsystem_count = 0;

void subsystem_reset(void) {
    subsystem_count = 0;
    memset(subsystem_list, 0, sizeof(subsystem_list));
}

void subsystem_store(const struct retro_subsystem_info *info) {
    subsystem_reset();
    if (!info) return;

    for (int i = 0; info[i].ident && subsystem_count < SUBSYSTEM_MAX; i++) {
        struct subsystem_entry *e = &subsystem_list[subsystem_count];

        snprintf(e->ident, sizeof(e->ident), "%s", info[i].ident);
        snprintf(e->desc, sizeof(e->desc), "%s", info[i].desc ? info[i].desc : info[i].ident);
        e->id = info[i].id;
        e->rom_count = 0;

        for (unsigned r = 0; r < info[i].num_roms && e->rom_count < SUBSYSTEM_ROM_MAX; r++) {
            struct subsystem_rom *rom = &e->roms[e->rom_count];

            snprintf(rom->desc, sizeof(rom->desc), "%s", info[i].roms[r].desc ? info[i].roms[r].desc : "");
            snprintf(
                rom->valid_extensions, sizeof(rom->valid_extensions), "%s",
                info[i].roms[r].valid_extensions ? info[i].roms[r].valid_extensions : ""
            );
            rom->required = info[i].roms[r].required ? 1 : 0;
            e->rom_count++;
        }

        if ((int) info[i].num_roms > e->rom_count)
            LOG_WARN(
                mux_module, "subsystem '%s' wants %u files, only %d are supported", e->ident, info[i].num_roms,
                e->rom_count
            );

        subsystem_count++;
    }

    if (info[subsystem_count].ident) LOG_WARN(mux_module, "subsystem list truncated at %d entries", SUBSYSTEM_MAX);
}

int subsystem_find(const char *ident) {
    if (!ident) return -1;

    for (int i = 0; i < subsystem_count; i++) {
        if (strcmp(subsystem_list[i].ident, ident) == 0) return i;
    }

    return -1;
}

void subsystem_log_resolved(void) {
    if (subsystem_count <= 0) return;

    LOG_INFO(mux_module, "core offers %d subsystem(s)", subsystem_count);
    for (int i = 0; i < subsystem_count; i++) {
        const struct subsystem_entry *e = &subsystem_list[i];
        LOG_INFO(mux_module, "  %s (%s) takes %d file(s)", e->ident, e->desc, e->rom_count);

        for (int r = 0; r < e->rom_count; r++)
            LOG_INFO(
                mux_module, "    %s [%s]%s", e->roms[r].desc, e->roms[r].valid_extensions,
                e->roms[r].required ? "" : " optional"
            );
    }
}

static char pending_paths[SUBSYSTEM_ROM_MAX][MAX_BUFFER_SIZE];
static int pending_count = 0;
static int pending_entry = -1;

int subsystem_pending_count(void) {
    return pending_count;
}

int subsystem_select(const char *ident, const char *const *paths, const int count) {
    const int index = subsystem_find(ident);
    if (index < 0 || count <= 0 || count > SUBSYSTEM_ROM_MAX) return 0;

    for (int i = 0; i < count; i++)
        snprintf(pending_paths[i], sizeof(pending_paths[i]), "%s", paths[i]);

    pending_count = count;
    pending_entry = index;
    return 1;
}

void subsystem_clear_pending(void) {
    pending_count = 0;
    pending_entry = -1;
}

const char *subsystem_pending_path(const int index) {
    if (index < 0 || index >= pending_count) return NULL;
    return pending_paths[index];
}

int subsystem_pending_entry(void) {
    return pending_entry;
}
