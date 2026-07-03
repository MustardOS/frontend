#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cache.h"
#include "../init.h"
#include "../theme.h"
#include "../config.h"
#include "../content.h"
#include "../log.h"

#define ASSET_CACHE_SLOTS 1024
#define ASSET_CACHE_MAX   512

_Static_assert(ASSET_CACHE_SLOTS >= ASSET_CACHE_MAX * 2, "ASSET_CACHE_SLOTS must be >= 2 * ASSET_CACHE_MAX");
_Static_assert((ASSET_CACHE_SLOTS & (ASSET_CACHE_SLOTS - 1)) == 0, "ASSET_CACHE_SLOTS must be a power of 2");

typedef struct {
    uint32_t hash;
    char key[MAX_BUFFER_SIZE];
    char path[MAX_BUFFER_SIZE];
    uint8_t found;
    uint8_t used;
} asset_cache_t;

static asset_cache_t asset_cache[ASSET_CACHE_SLOTS];
static int asset_cache_count = 0;
static uint32_t last_asset_context_hash = 0;

int asset_cache_get(const char *key, char *path_out, const size_t path_out_size) {
    const uint32_t h = fnv_hash_str(key);
    const uint32_t slot = h & (ASSET_CACHE_SLOTS - 1);

    for (int i = 0; i < ASSET_CACHE_SLOTS; i++) {
        const asset_cache_t *e = &asset_cache[(slot + i) & (ASSET_CACHE_SLOTS - 1)];
        if (!e->used) return -1;

        if (e->hash == h && strcmp(e->key, key) == 0) {
            if (path_out && path_out_size) snprintf(path_out, path_out_size, "%s", e->found ? e->path : "");
            return e->found ? 1 : 0;
        }
    }

    return -1;
}

void asset_cache_put(const char *key, const char *path, const int found) {
    if (asset_cache_count >= ASSET_CACHE_MAX) return;

    const uint32_t h = fnv_hash_str(key);
    const uint32_t slot = h & (ASSET_CACHE_SLOTS - 1);

    for (int i = 0; i < ASSET_CACHE_SLOTS; i++) {
        asset_cache_t *e = &asset_cache[(slot + i) & (ASSET_CACHE_SLOTS - 1)];
        if (e->used) continue;

        e->used = 1;
        e->hash = h;
        e->found = (uint8_t) (found != 0);

        snprintf(e->key, sizeof(e->key), "%s", key);
        snprintf(e->path, sizeof(e->path), "%s", found ? path : "");

        asset_cache_count++;
        return;
    }

    LOG_WARN(mux_module, "Asset cache hash table unexpectedly full; discarding: %s", key);
}

void asset_cache_clear(void) {
    memset(asset_cache, 0, sizeof(asset_cache));
    asset_cache_count = 0;
}

int asset_cache_context_changed(void) {
    uint32_t h = fnv_hash_str(theme_base);
    h ^= fnv_hash_str(mux_dim);
    h *= 16777619u;

    h ^= fnv_hash_str(config.settings.general.language);
    h *= 16777619u;

    if (h == last_asset_context_hash) return 0;
    last_asset_context_hash = h;

    return 1;
}
