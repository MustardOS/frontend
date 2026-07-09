#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../common/fileio.h"
#include "../common/language.h"
#include "../common/mini/mini.h"
#include "../common/miniz/miniz.h"
#include "content_hash.h"
#include "paths.h"

#define HASH_READ_CHUNK 65536

static volatile int hash_running = 0;
static volatile int hash_ready = 0;
static char hash_result[16] = "";
static char hash_active_path[PATH_MAX] = "";

static char cache_path[MAX_BUFFER_SIZE] = "";

#define CACHE_SECTION "hash"

static void set_cache_path(const char *content_path) {
    const char *name = strrchr(content_path, '/');
    name = name ? name + 1 : content_path;

    char stem[MAX_BUFFER_SIZE];
    snprintf(stem, sizeof(stem), "%s", name);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    snprintf(cache_path, sizeof(cache_path), "%s/%s.ini", RETRO_HSH_PATH, stem);
    create_directories(cache_path, 1);
}

static int cache_read(const long long size, const long long mtime, char *out, const size_t out_size) {
    mini_t *ini = mini_try_load(cache_path);
    if (!ini) return 0;

    const long long cached_size = mini_get_int(ini, CACHE_SECTION, "size", -1);
    const long long cached_mtime = mini_get_int(ini, CACHE_SECTION, "mtime", -1);
    const char *cached_crc = mini_get_string(ini, CACHE_SECTION, "crc32", "");

    int ok = 0;
    if (cached_size == size && cached_mtime == mtime && *cached_crc) {
        snprintf(out, out_size, "%s", cached_crc);
        ok = 1;
    }

    mini_free(ini);
    return ok;
}

static void cache_write(const long long size, const long long mtime, const char *crc) {
    mini_t *ini = mini_try_load(cache_path);
    if (!ini) ini = mini_create(cache_path);
    if (!ini) return;

    mini_set_int(ini, CACHE_SECTION, "size", size);
    mini_set_int(ini, CACHE_SECTION, "mtime", mtime);
    mini_set_string(ini, CACHE_SECTION, "crc32", crc);

    mini_save(ini, 0);
    mini_free(ini);
}

static void finish(const char *path, const char *result) {
    snprintf(hash_result, sizeof(hash_result), "%s", result);
    hash_ready = 1;
    hash_running = 0;
    free((void *) path);
}

static void *compute_thread(void *arg) {
    const char *path = arg;

    struct stat st;
    if (stat(path, &st) != 0) {
        finish(path, lang.generic.unknown);
        return NULL;
    }

    char cached[16];
    if (cache_read(st.st_size, st.st_mtime, cached, sizeof(cached))) {
        finish(path, cached);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        finish(path, lang.generic.unknown);
        return NULL;
    }

    mz_ulong crc = MZ_CRC32_INIT;
    unsigned char *buf = malloc(HASH_READ_CHUNK);
    if (buf) {
        size_t n;
        while ((n = fread(buf, 1, HASH_READ_CHUNK, f)) > 0)
            crc = mz_crc32(crc, buf, n);
        free(buf);
    }
    fclose(f);

    char result[16];
    snprintf(result, sizeof(result), "%08lX", (unsigned long) crc);
    cache_write(st.st_size, st.st_mtime, result);

    finish(path, result);
    return NULL;
}

void content_hash_request(const char *content_path) {
    if (hash_running) return;
    if (hash_ready && strcmp(hash_active_path, content_path) == 0) return;

    set_cache_path(content_path);

    snprintf(hash_active_path, sizeof(hash_active_path), "%s", content_path);
    hash_ready = 0;
    hash_result[0] = '\0';

    char *path_copy = strdup(content_path);
    if (!path_copy) {
        snprintf(hash_result, sizeof(hash_result), "%s", lang.generic.unknown);
        hash_ready = 1;
        return;
    }

    hash_running = 1;

    pthread_t tid;
    if (pthread_create(&tid, NULL, compute_thread, path_copy) != 0) {
        free(path_copy);
        hash_running = 0;
        snprintf(hash_result, sizeof(hash_result), "%s", lang.generic.unknown);
        hash_ready = 1;
        return;
    }
    pthread_detach(tid);
}

int content_hash_is_ready(void) {
    return hash_ready;
}

const char *content_hash_get(void) {
    return hash_result;
}
