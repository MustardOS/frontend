#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <common/storage/fileio.h>
#include <common/runtime/init.h>
#include <common/display/language.h>
#include <common/runtime/log.h>
#include <mini/mini.h>
#include <miniz/miniz.h>
#include "content_hash.h"
#include "../cheevo/cheevo.h"
#include "../core/paths.h"

#define HASH_READ_CHUNK 65536
#define HASH_VALUE_CAP  40

static pthread_mutex_t hash_mutex = PTHREAD_MUTEX_INITIALIZER;
static int hash_running = 0;
static int hash_ready[content_hash_count] = {0};
static char hash_result[content_hash_count][HASH_VALUE_CAP] = {{0}};
static char hash_active_path[PATH_MAX] = "";

static char cache_path[MAX_BUFFER_SIZE] = "";

typedef struct {
    char archive_path[PATH_MAX];
    char content_path[PATH_MAX];
} hash_job_t;

static const char *cache_section(const enum content_hash_kind kind) {
    switch (kind) {
        case content_hash_archive:
            return "archive";
        case content_hash_cheevo:
            return "cheevo";
        default:
            return "content";
    }
}

static void build_cache_path(const char *content_path, char *out, const size_t out_size) {
    const char *name = strrchr(content_path, '/');
    name = name ? name + 1 : content_path;

    char stem[MAX_BUFFER_SIZE];
    snprintf(stem, sizeof(stem), "%s", name);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    snprintf(out, out_size, "%s/%s.ini", RETRO_HSH_PATH, stem);
    create_directories(out, 1);
}

static void set_cache_path(const char *content_path) {
    build_cache_path(content_path, cache_path, sizeof(cache_path));
}

static int cache_read(
    const char *path, const enum content_hash_kind kind, const long long size, const long long mtime, char *out
) {
    mini_t *ini = mini_try_load(path);
    if (!ini) return 0;

    const char *section = cache_section(kind);
    const long long cached_size = mini_get_int(ini, section, "size", -1);
    const long long cached_mtime = mini_get_int(ini, section, "mtime", -1);
    const char *cached_value = mini_get_string(ini, section, "value", "");

    int ok = 0;
    if (cached_size == size && cached_mtime == mtime && *cached_value) {
        snprintf(out, HASH_VALUE_CAP, "%s", cached_value);
        ok = 1;
    }

    mini_free(ini);
    return ok;
}

static void cache_write(
    const char *path, const enum content_hash_kind kind, const long long size, const long long mtime, const char *value
) {
    mini_t *ini = mini_try_load(path);
    if (!ini) ini = mini_create(path);
    if (!ini) return;

    const char *section = cache_section(kind);
    mini_set_int(ini, section, "size", size);
    mini_set_int(ini, section, "mtime", mtime);
    mini_set_string(ini, section, "value", value);

    if (mini_save(ini, 0) != MINI_OK) LOG_WARN(mux_module, "Could not safely update content hash cache: %s", path);
    mini_free(ini);
}

static void publish(const enum content_hash_kind kind, const char *value) {
    pthread_mutex_lock(&hash_mutex);
    snprintf(hash_result[kind], HASH_VALUE_CAP, "%s", value);
    hash_ready[kind] = 1;
    pthread_mutex_unlock(&hash_mutex);
}

static int crc32_file(const char *path, char *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    mz_ulong crc = MZ_CRC32_INIT;
    unsigned char *buf = malloc(HASH_READ_CHUNK);
    if (!buf) {
        fclose(f);
        return -1;
    }

    size_t n;
    while ((n = fread(buf, 1, HASH_READ_CHUNK, f)) > 0)
        crc = mz_crc32(crc, buf, n);

    free(buf);
    fclose(f);

    snprintf(out, HASH_VALUE_CAP, "%08lX", (unsigned long) crc);
    return 0;
}

static int stat_for_kind(const char *path, struct stat *st) {
    if (stat(path, st) == 0) return 0;

    const char *sep = strrchr(path, '#');
    if (!sep) return -1;

    char container[PATH_MAX];
    const size_t len = (size_t) (sep - path);
    if (len >= sizeof(container)) return -1;

    memcpy(container, path, len);
    container[len] = '\0';

    return stat(container, st);
}

static int compute_value(const enum content_hash_kind kind, const char *path, char *value) {
    if (kind != content_hash_cheevo) return crc32_file(path, value);

    char md5[33];
    if (!cheevo_hash_content(path, md5)) return -1;

    snprintf(value, HASH_VALUE_CAP, "%s", md5);
    return 0;
}

static void compute_one(const enum content_hash_kind kind, const char *path) {
    struct stat st;
    if (!path[0] || stat_for_kind(path, &st) != 0) {
        publish(kind, lang.generic.unknown);
        return;
    }

    char value[HASH_VALUE_CAP];
    if (cache_read(cache_path, kind, st.st_size, st.st_mtime, value)) {
        publish(kind, value);
        return;
    }

    if (compute_value(kind, path, value) != 0) {
        publish(kind, lang.generic.unknown);
        return;
    }

    cache_write(cache_path, kind, st.st_size, st.st_mtime, value);
    publish(kind, value);
}

static void *compute_thread(void *arg) {
    hash_job_t *job = arg;

    compute_one(content_hash_archive, job->archive_path);
    compute_one(content_hash_content, job->content_path);
    compute_one(content_hash_cheevo, job->content_path);

    pthread_mutex_lock(&hash_mutex);
    hash_running = 0;
    pthread_mutex_unlock(&hash_mutex);

    free(job);
    return NULL;
}

void content_hash_request(const char *archive_path, const char *content_path) {
    if (!archive_path || !archive_path[0]) return;

    pthread_mutex_lock(&hash_mutex);
    const int running = hash_running;
    const int already = hash_ready[content_hash_cheevo] && strcmp(hash_active_path, archive_path) == 0;
    pthread_mutex_unlock(&hash_mutex);
    if (running || already) return;

    set_cache_path(archive_path);
    snprintf(hash_active_path, sizeof(hash_active_path), "%s", archive_path);

    hash_job_t *job = calloc(1, sizeof(*job));
    if (!job) return;

    snprintf(job->archive_path, sizeof(job->archive_path), "%s", archive_path);
    snprintf(job->content_path, sizeof(job->content_path), "%s", content_path ? content_path : archive_path);

    pthread_mutex_lock(&hash_mutex);
    for (int i = 0; i < content_hash_count; i++) {
        hash_ready[i] = 0;
        hash_result[i][0] = '\0';
    }
    hash_running = 1;
    pthread_mutex_unlock(&hash_mutex);

    pthread_t tid;
    if (pthread_create(&tid, NULL, compute_thread, job) != 0) {
        free(job);

        pthread_mutex_lock(&hash_mutex);
        hash_running = 0;
        for (int i = 0; i < content_hash_count; i++) {
            snprintf(hash_result[i], HASH_VALUE_CAP, "%s", lang.generic.unknown);
            hash_ready[i] = 1;
        }
        pthread_mutex_unlock(&hash_mutex);
        return;
    }
    pthread_detach(tid);
}

int content_hash_is_ready(const enum content_hash_kind kind) {
    pthread_mutex_lock(&hash_mutex);
    const int ready = hash_ready[kind];
    pthread_mutex_unlock(&hash_mutex);
    return ready;
}

const char *content_hash_get(const enum content_hash_kind kind) {
    return hash_result[kind];
}

int content_hash_resolve(
    const enum content_hash_kind kind, const char *archive_path, const char *path, const long long compute_limit,
    char *out, const size_t out_size
) {
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!archive_path || !archive_path[0] || !path || !path[0]) return 0;

    struct stat st;
    if (stat_for_kind(path, &st) != 0) return 0;

    char resolve_cache[MAX_BUFFER_SIZE];
    build_cache_path(archive_path, resolve_cache, sizeof(resolve_cache));

    char value[HASH_VALUE_CAP];
    if (!cache_read(resolve_cache, kind, st.st_size, st.st_mtime, value)) {
        if (compute_limit >= 0 && st.st_size > compute_limit) return 0;
        if (compute_value(kind, path, value) != 0) return 0;

        cache_write(resolve_cache, kind, st.st_size, st.st_mtime, value);
    }

    snprintf(out, out_size, "%s", value);
    return 1;
}

int content_hash_archive_cached(
    const enum content_hash_kind kind, const char *archive_path, char *out, const size_t out_size
) {
    if (!out || out_size == 0) return 0;
    out[0] = '\0';
    if (!archive_path || !archive_path[0]) return 0;

    struct stat st;
    if (stat(archive_path, &st) != 0) return 0;

    char resolve_cache[MAX_BUFFER_SIZE];
    build_cache_path(archive_path, resolve_cache, sizeof(resolve_cache));

    char archive_value[HASH_VALUE_CAP];
    if (!cache_read(resolve_cache, content_hash_archive, st.st_size, st.st_mtime, archive_value)) return 0;

    mini_t *ini = mini_try_load(resolve_cache);
    if (!ini) return 0;

    snprintf(out, out_size, "%s", mini_get_string(ini, cache_section(kind), "value", ""));
    mini_free(ini);
    return out[0] != '\0';
}
