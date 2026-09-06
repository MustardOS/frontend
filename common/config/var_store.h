#pragma once

#include <stdint.h>

#define VS_MAGIC   0x53565541u
#define VS_VERSION 2u
#define VS_KEY_MAX 128u
#define VS_VAL_MAX 448u
#define VS_DEF_CAP 2048u

typedef enum {
    vs_ns_global = 0, // /opt/muos/config
    vs_ns_device = 1, // /opt/muos/device/config
    vs_ns_kiosk = 2,  // /opt/muos/kiosk
    vs_ns_system = 3, // /opt/muos/config/system
    vs_ns_count
} var_ns_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t hash;
    uint8_t ns;
    uint8_t occupied;
    uint8_t dirty;
    uint8_t tombstone;
    char key[VS_KEY_MAX];
    char value[VS_VAL_MAX];
} var_slot_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;
    uint32_t count;
    uint32_t built_unix;
    uint32_t reserved[3];
} var_header_t;
#pragma pack(pop)

typedef struct {
    int fd;
    void *map;
    size_t map_size;
    var_header_t *hdr;
    var_slot_t *slots;
    int writable;
} var_store_t;

enum {
    vs_ok = 0,
    vs_notfound = 1,
    vs_err_inval = -1,
    vs_err_io = -2,
    vs_err_full = -3,
    vs_err_lock = -5,
};

typedef struct {
    const char *base[vs_ns_count];
} var_dirs_t;

int vs_open(var_store_t *vs, const char *path, int create_if_missing, uint32_t capacity_hint, int writable);

void vs_close(var_store_t *vs);

int vs_valid_key(const char *key);

uint32_t vs_hash(var_ns_t ns, const char *key);

void vs_copy_out(char *dst, size_t dst_sz, const char *src, size_t src_sz);

int vs_canon(var_ns_t ns, const char *key, var_ns_t *out_ns, char *out_key, size_t out_sz);

int vs_get(const var_store_t *vs, var_ns_t ns, const char *key, char *out, size_t out_sz);

int vs_lock_shared(const var_store_t *vs);

void vs_unlock(const var_store_t *vs);

int vs_get_locked(const var_store_t *vs, var_ns_t ns, const char *key, char *out, size_t out_sz);

int vs_set(const var_store_t *vs, var_ns_t ns, const char *key, const char *value);

int vs_write(const var_dirs_t *dirs, var_ns_t ns, const char *key, const char *value);

int vs_store(const var_store_t *vs, const var_dirs_t *dirs, var_ns_t ns, const char *key, const char *value);

int vs_del(const var_store_t *vs, var_ns_t ns, const char *key);

int vs_build(var_store_t *vs, const var_dirs_t *dirs);

int vs_flush(const var_store_t *vs, const var_dirs_t *dirs);

const char *vs_cache_path(void);

void vs_default_dirs(var_dirs_t *dirs);

void vs_invalidate_path(const char *path);
