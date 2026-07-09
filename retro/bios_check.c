#include <stdio.h>
#include <string.h>
#include "../common/fileio.h"
#include "../common/options.h"
#include "../common/strutil.h"
#include "bios_check.h"

#define CORE_INFO_PATH OPT_SHARE_PATH "emulator/retroarch/info/"

static bios_entry_t entries[BIOS_CHECK_MAX];
static int entry_count = 0;

int bios_check_scan(const char *core_file_path) {
    entry_count = 0;

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
        entries[i].present = file_exist(bios_path);
    }

    return entry_count;
}

const bios_entry_t *bios_check_get(const int index) {
    if (index < 0 || index >= entry_count) return NULL;
    return &entries[index];
}
