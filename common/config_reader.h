#pragma once

#define CFG_DIR_MAX   64
#define CFG_NAME_MAX  64
#define CFG_VALUE_MAX 1024

typedef struct {
    char name[CFG_NAME_MAX];
    char value[CFG_VALUE_MAX];
} cfg_kv_t;

typedef struct {
    cfg_kv_t entries[CFG_DIR_MAX];
    int count;
} cfg_dir_t;

void cfg_dir_scan(cfg_dir_t *d, const char *dir_path);

const char *cfg_dir_get(const cfg_dir_t *d, const char *name);

int cfg_dir_int(const cfg_dir_t *d, const char *name, int fallback);

double cfg_dir_flo(const cfg_dir_t *d, const char *name, double fallback);
