#include <stdio.h>
#include <string.h>
#include <common/storage/fileio.h>
#include <common/base/options.h>
#include <common/base/strutil.h>
#include "bios_check.h"

#define CORE_INFO_PATH OPT_SHARE_PATH "emulator/retroarch/info/"

static bios_entry_t entries[BIOS_CHECK_MAX];
static int entry_count = 0;

int bios_check_scan(const char *core_file_path) {
    entry_count = 0;
    memset(entries, 0, sizeof(entries));

    const char *base = strrchr(core_file_path, '/');
    base = base ? base + 1 : core_file_path;

    char core_name[128];
    snprintf(core_name, sizeof(core_name), "%s", base);
    char *ext = strstr(core_name, ".so");
    if (ext) *ext = '\0';

    char info_path[MAX_BUFFER_SIZE];
    snprintf(info_path, sizeof(info_path), "%s%s.info", CORE_INFO_PATH, core_name);

    FILE *f = fopen(info_path, "r");
    if (!f) return 0;

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

        int idx;
        char field[16];
        if (strncmp(key, "firmware", 8) != 0 || sscanf(key, "firmware%d_%15s", &idx, field) != 2) continue;
        if (idx < 0 || idx >= BIOS_CHECK_MAX) continue;
        if (idx >= entry_count) entry_count = idx + 1;

        if (strcmp(field, "desc") == 0) {
            snprintf(entries[idx].desc, sizeof(entries[idx].desc), "%s", val);
        } else if (strcmp(field, "path") == 0) {
            snprintf(entries[idx].filename, sizeof(entries[idx].filename), "%s", val);
        } else if (strcmp(field, "opt") == 0) {
            entries[idx].optional = strcmp(val, "true") == 0;
        }
    }
    fclose(f);

    for (int i = 0; i < entry_count; i++) {
        char bios_path[MAX_BUFFER_SIZE];
        snprintf(bios_path, sizeof(bios_path), "%s/%s", STORAGE_BIOS, entries[i].filename);
        entries[i].present = file_exist_nocase(bios_path, NULL, 0);
    }

    return entry_count;
}

static void append_name(char *list, const size_t list_size, const char *name) {
    if (!list || !list_size || !name || !*name) return;

    const size_t used = strlen(list);
    if (used + 1 >= list_size) return;

    snprintf(list + used, list_size - used, "%s%s", used ? "\n" : "", name);
}

static const char *const bios_only_cores[] = {
    "4do",
    "cdi2015",
    "duckstation",
    "freechaf",
    "freeintv",
    "gearcoleco",
    "geolith",
    "kronos",
    "mednafen_lynx",
    "mednafen_pcfx",
    "mednafen_psx",
    "mednafen_psx_hw",
    "mednafen_saturn",
    "minivmac",
    "mu",
    "neocd",
    "numero",
    "o2em",
    "opera",
    "pcfx",
    "pcsx2",
    "px68k",
    "retrodream",
    "same_cdi",
    "swanstation",
    "yabadabadoo",
};

static int core_needs_bios(const char *core_file_path) {
    const char *base = strrchr(core_file_path, '/');
    base = base ? base + 1 : core_file_path;

    for (size_t i = 0; i < sizeof(bios_only_cores) / sizeof(bios_only_cores[0]); i++) {
        const size_t len = strlen(bios_only_cores[i]);
        if (strncmp(base, bios_only_cores[i], len) == 0 && strcmp(base + len, "_libretro.so") == 0) return 1;
    }

    return 0;
}

int bios_check_missing(const char *core_file_path, char *list, const size_t list_size) {
    if (list && list_size) list[0] = '\0';
    if (!core_needs_bios(core_file_path)) return 0;

    const int count = bios_check_scan(core_file_path);
    if (count <= 0) return 0;

    int required = 0;
    for (int i = 0; i < count; i++)
        if (!entries[i].optional && entries[i].filename[0]) required++;

    for (int i = 0; i < count; i++) {
        if (!entries[i].filename[0] || (required && entries[i].optional)) continue;
        if (entries[i].present) return 0;
    }

    int listed = 0;
    for (int i = 0; i < count; i++) {
        if (!entries[i].filename[0] || (required && entries[i].optional)) continue;

        append_name(list, list_size, entries[i].filename);
        listed++;
    }

    return listed;
}

const bios_entry_t *bios_check_get(const int index) {
    if (index < 0 || index >= entry_count) return NULL;
    return &entries[index];
}
