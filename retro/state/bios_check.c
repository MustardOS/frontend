#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/base/options.h>
#include <common/content/core/coredb.h>
#include <common/display/language.h>
#include <common/storage/fileio.h>
#include "../core/paths.h"
#include "bios_check.h"

static bios_entry_t entries[BIOS_CHECK_MAX];
static int entry_count = 0;

static const char *core_file_name(const char *core_file_path) {
    const char *core_file = strrchr(core_file_path, '/');
    return core_file ? core_file + 1 : core_file_path;
}

static int ignore_path(const char *core_file_path, char *out, const size_t out_size) {
    const char *system = getenv(BIOS_CHECK_SYSTEM_ENV);
    if (!system || !*system || !core_file_path) return 0;

    snprintf(out, out_size, "%s/%s/%s", RETRO_BIG_PATH, system, core_file_name(core_file_path));
    return 1;
}

int bios_check_ignored(const char *core_file_path) {
    char path[MAX_BUFFER_SIZE];
    return ignore_path(core_file_path, path, sizeof(path)) && file_exist(path);
}

void bios_check_ignore(const char *core_file_path) {
    char path[MAX_BUFFER_SIZE];
    if (!ignore_path(core_file_path, path, sizeof(path))) return;

    create_directories(path, 1);
    write_text_to_file(path, "w", CHAR, "");
}

int bios_check_scan(const char *core_file_path) {
    entry_count = 0;
    memset(entries, 0, sizeof(entries));

    const char *system = getenv(BIOS_CHECK_SYSTEM_ENV);
    if (!system || !*system || !core_file_path) return 0;

    const char *core_file = core_file_name(core_file_path);

    const int required = coredb_system_core_bios_required(system, core_file, core_runtime_pickles);

    while (entry_count < BIOS_CHECK_MAX) {
        bios_entry_t *entry = &entries[entry_count];
        if (!coredb_system_bios_at(system, entry_count, entry->filename, sizeof(entry->filename))) break;

        char bios_path[MAX_BUFFER_SIZE];
        snprintf(bios_path, sizeof(bios_path), "%s/%s", STORAGE_BIOS, entry->filename);

        entry->optional = !required;
        entry->present = file_exist_nocase(bios_path, NULL, 0);
        entry_count++;
    }

    return entry_count;
}

static void append_name(char *list, const size_t list_size, const char *name) {
    if (!list || !list_size || !name || !*name) return;

    const size_t used = strlen(list);
    if (used + 1 >= list_size) return;

    snprintf(list + used, list_size - used, "%s%s", used ? "\n" : "", name);
}

int bios_check_missing(const char *core_file_path, char *list, const size_t list_size) {
    if (list && list_size) list[0] = '\0';
    if (bios_check_scan(core_file_path) <= 0 || entries[0].optional) return 0;

    for (int i = 0; i < entry_count; i++)
        if (entries[i].present) return 0;

    const int shown = entry_count > BIOS_CHECK_SHOWN + 1 ? BIOS_CHECK_SHOWN : entry_count;
    for (int i = 0; i < shown; i++)
        append_name(list, list_size, entries[i].filename);

    if (shown < entry_count) {
        char others[MAX_BUFFER_SIZE];
        snprintf(others, sizeof(others), lang.muxretro.bios_others, entry_count - shown);
        append_name(list, list_size, others);
    }

    return entry_count;
}

const bios_entry_t *bios_check_get(const int index) {
    if (index < 0 || index >= entry_count) return NULL;
    return &entries[index];
}
