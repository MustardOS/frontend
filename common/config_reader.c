#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "config_reader.h"

void cfg_dir_scan(cfg_dir_t *d, const char *dir_path) {
    d->count = 0;

    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && d->count < CFG_DIR_MAX) {
        if (ent->d_name[0] == '.') continue;

        // Fast-path skip for known directory entries (works on ext4/tmpfs).
        // Fall through to stat for DT_UNKNOWN (FAT32, some kernels).
        if (ent->d_type != DT_UNKNOWN && ent->d_type != DT_REG) continue;

        char path[512];
        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name);
        if (n <= 0 || (size_t) n >= sizeof(path)) continue;

        if (ent->d_type == DT_UNKNOWN) {
            struct stat st;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        }

        FILE *f = fopen(path, "r");
        if (!f) continue;

        cfg_kv_t *kv = &d->entries[d->count];

        size_t nlen = strnlen(ent->d_name, CFG_NAME_MAX - 1);
        memcpy(kv->name, ent->d_name, nlen);
        kv->name[nlen] = '\0';

        kv->value[0] = '\0';
        if (fgets(kv->value, sizeof(kv->value), f)) {
            size_t vlen = strlen(kv->value);
            while (vlen > 0 &&
                   (kv->value[vlen - 1] == '\n' || kv->value[vlen - 1] == '\r')) {
                kv->value[--vlen] = '\0';
            }
        }

        fclose(f);
        d->count++;
    }

    closedir(dir);
}

const char *cfg_dir_get(const cfg_dir_t *d, const char *name) {
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->entries[i].name, name) == 0) return d->entries[i].value;
    }

    return NULL;
}

int cfg_dir_int(const cfg_dir_t *d, const char *name, int fallback) {
    const char *v = cfg_dir_get(d, name);
    if (!v || !*v) return fallback;

    char *end;
    long val = strtol(v, &end, 10);

    return (end != v && *end == '\0') ? (int) val : fallback;
}

double cfg_dir_flo(const cfg_dir_t *d, const char *name, double fallback) {
    const char *v = cfg_dir_get(d, name);
    if (!v || !*v) return fallback;

    char *end;
    double val = strtod(v, &end);

    return (end != v && *end == '\0') ? val : fallback;
}
