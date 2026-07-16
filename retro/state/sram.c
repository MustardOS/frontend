#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/miniz/miniz.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "../settings/settings.h"
#include "sram.h"

#define SRAM_BACKUP_MAX 10

static char sram_path[MAX_BUFFER_SIZE] = "";

static uint8_t *shadow_buf = NULL;
static size_t shadow_cap = 0;
static size_t shadow_size = 0;

static uint8_t *job_buf = NULL;
static size_t job_cap = 0;
static size_t job_size = 0;
static char job_path[MAX_BUFFER_SIZE] = "";
static int job_pending = 0;

static uint8_t *worker_buf = NULL;
static size_t worker_cap = 0;

static pthread_mutex_t sram_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sram_cond = PTHREAD_COND_INITIALIZER;
static pthread_t sram_thread;

static int sram_thread_running = 0;
static int worker_shutdown = 0;
static int write_in_flight = 0;

static int atomic_write_file(const char *path, const void *data, const size_t size) {
    char tmp_path[MAX_BUFFER_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        LOG_ERROR(mux_module, "Failed to open '%s' for writing", tmp_path);
        return -1;
    }

    const int ok = fwrite(data, 1, size, f) == size;
    if (ok) {
        fflush(f);
        fdatasync(fileno(f));
    }
    fclose(f);

    if (!ok) {
        LOG_ERROR(mux_module, "Failed to write '%s'", tmp_path);
        remove(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) != 0) {
        LOG_ERROR(mux_module, "Failed to rename '%s' to '%s'", tmp_path, path);
        return -1;
    }

    char dir_path[MAX_BUFFER_SIZE];
    snprintf(dir_path, sizeof(dir_path), "%s", path);
    char *slash = strrchr(dir_path, '/');
    if (slash) *slash = '\0';

    const int dir_fd = open(dir_path, O_RDONLY);
    if (dir_fd >= 0) {
        fsync(dir_fd);
        close(dir_fd);
    }

    return 0;
}

static void sum_path_for(const char *path, char *out, const size_t out_size) {
    snprintf(out, out_size, "%s.sum", path);
}

static uint32_t compute_crc(const void *data, const size_t size) {
    return (uint32_t) mz_crc32(MZ_CRC32_INIT, data, size);
}

// Writes a CRC32 checksum next to path so a later load can tell whether or not the path
// contents are still what was written rather than dealing with corrupted data.
static void write_checksum(const char *path, const void *data, const size_t size) {
    char sum_path[MAX_BUFFER_SIZE];
    sum_path_for(path, sum_path, sizeof(sum_path));

    FILE *f = fopen(sum_path, "w");
    if (!f) return;

    fprintf(f, "%08X", compute_crc(data, size));
    fclose(f);
}

// Because of how this will work we'll only check the sum if the sum exists.
// Otherwise manually imported SRAM data from RA for example will be marked
// as invalid and we don't want that headache...
static int checksum_matches(const char *path, const void *data, const size_t size) {
    char sum_path[MAX_BUFFER_SIZE];
    sum_path_for(path, sum_path, sizeof(sum_path));

    FILE *f = fopen(sum_path, "r");
    if (!f) return 1;

    char stored[16] = "";
    const int got = fscanf(f, "%15s", stored);
    fclose(f);
    if (got != 1) return 1;

    char expect[16];
    snprintf(expect, sizeof(expect), "%08X", compute_crc(data, size));

    return strcasecmp(stored, expect) == 0;
}

static int read_whole_sram_file(const char *path, void *data, const size_t size, size_t *out_read) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    const long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int ok = 0;
    if (file_size > 0) {
        const size_t to_read = (size_t) file_size < size ? (size_t) file_size : size;
        if (fread(data, 1, to_read, f) == to_read) {
            if (out_read) *out_read = to_read;
            ok = 1;
        }
    }

    fclose(f);
    return ok;
}

// Rotate the SRAM backup with 'bk0' being the newset and 'bk9' being the oldest.
static void rotate_sram_backups(const char *path) {
    if (!session_settings.sram_backup_enabled) return;
    if (!file_exist(path)) return;

    char oldest[MAX_BUFFER_SIZE];
    char oldest_sum[MAX_BUFFER_SIZE];
    snprintf(oldest, sizeof(oldest), "%s.bk%d", path, SRAM_BACKUP_MAX - 1);
    snprintf(oldest_sum, sizeof(oldest_sum), "%s.bk%d.sum", path, SRAM_BACKUP_MAX - 1);
    remove(oldest);
    remove(oldest_sum);

    for (int i = SRAM_BACKUP_MAX - 2; i >= 0; i--) {
        char from[MAX_BUFFER_SIZE];
        char to[MAX_BUFFER_SIZE];
        char from_sum[MAX_BUFFER_SIZE];
        char to_sum[MAX_BUFFER_SIZE];
        snprintf(from, sizeof(from), "%s.bk%d", path, i);
        snprintf(to, sizeof(to), "%s.bk%d", path, i + 1);
        snprintf(from_sum, sizeof(from_sum), "%s.bk%d.sum", path, i);
        snprintf(to_sum, sizeof(to_sum), "%s.bk%d.sum", path, i + 1);
        rename(from, to);
        rename(from_sum, to_sum);
    }

    char bk0[MAX_BUFFER_SIZE];
    char bk0_sum[MAX_BUFFER_SIZE];
    char path_sum[MAX_BUFFER_SIZE];
    snprintf(bk0, sizeof(bk0), "%s.bk0", path);
    snprintf(bk0_sum, sizeof(bk0_sum), "%s.bk0.sum", path);
    sum_path_for(path, path_sum, sizeof(path_sum));
    rename(path, bk0);
    rename(path_sum, bk0_sum);
}

static void *sram_worker_main(void *arg) {
    (void) arg;

    pthread_mutex_lock(&sram_mutex);

    for (;;) {
        while (!job_pending && !worker_shutdown)
            pthread_cond_wait(&sram_cond, &sram_mutex);

        if (!job_pending && worker_shutdown) break;

        if (job_size > worker_cap) {
            free(worker_buf);
            worker_buf = malloc(job_size);
            worker_cap = worker_buf ? job_size : 0;
        }

        size_t write_size = 0;
        char path[MAX_BUFFER_SIZE];
        if (worker_buf) {
            memcpy(worker_buf, job_buf, job_size);
            write_size = job_size;
            snprintf(path, sizeof(path), "%s", job_path);
        }

        job_pending = 0;
        write_in_flight = 1;
        pthread_mutex_unlock(&sram_mutex);

        if (write_size > 0) {
            rotate_sram_backups(path);
            if (atomic_write_file(path, worker_buf, write_size) == 0) {
                write_checksum(path, worker_buf, write_size);
            }
        }

        pthread_mutex_lock(&sram_mutex);
        write_in_flight = 0;
        pthread_cond_broadcast(&sram_cond);
    }

    pthread_mutex_unlock(&sram_mutex);
    return NULL;
}

void sram_bridge_init(const char *core_path_arg, const char *content_path) {
    sram_path[0] = '\0';

    if (!current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    const size_t size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *data = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return;

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    core_content_save_prefix(core_path_arg, content_path, save_prefix, sizeof(save_prefix));

    snprintf(sram_path, sizeof(sram_path), "%s/%s/%s.srm", RETRO_SRM_PATH, save_prefix, content_stem);
    create_directories(sram_path, 1);

    int have_valid = 0;
    size_t loaded_size = 0;

    uint8_t *scratch = malloc(size);
    if (!scratch) {
        LOG_ERROR(mux_module, "Failed to allocate SRAM validation buffer (%zu bytes)", size);
    } else {
        if (file_exist(sram_path)) {
            size_t candidate_size = 0;
            if (read_whole_sram_file(sram_path, scratch, size, &candidate_size)
                && checksum_matches(sram_path, scratch, candidate_size)) {
                memcpy(data, scratch, candidate_size);
                have_valid = 1;
                loaded_size = candidate_size;
                LOG_SUCCESS(mux_module, "Loaded SRAM: %s (%zu bytes)", sram_path, loaded_size);
            } else {
                LOG_ERROR(mux_module, "SRAM failed validation, assuming corruption: %s", sram_path);
            }
        }

        if (!have_valid) {
            for (int i = 0; i < SRAM_BACKUP_MAX; i++) {
                char backup_path[MAX_BUFFER_SIZE];
                snprintf(backup_path, sizeof(backup_path), "%s.bk%d", sram_path, i);
                if (!file_exist(backup_path)) continue;

                size_t candidate_size = 0;
                if (read_whole_sram_file(backup_path, scratch, size, &candidate_size)
                    && checksum_matches(backup_path, scratch, candidate_size)) {
                    memcpy(data, scratch, candidate_size);
                    have_valid = 1;
                    loaded_size = candidate_size;
                    LOG_SUCCESS(mux_module, "Recovered SRAM from backup: %s (%zu bytes)", backup_path, loaded_size);
                    break;
                }
            }

            if (have_valid) {
                if (atomic_write_file(sram_path, data, loaded_size) == 0) {
                    write_checksum(sram_path, data, loaded_size);
                }
            } else if (file_exist(sram_path)) {
                LOG_ERROR(mux_module, "No valid SRAM or backup found for: %s", sram_path);
            }
        }

        free(scratch);
    }

    shadow_buf = malloc(size);
    if (shadow_buf) {
        memcpy(shadow_buf, data, size);
        shadow_cap = size;
        shadow_size = size;
    }

    worker_shutdown = 0;
    if (pthread_create(&sram_thread, NULL, sram_worker_main, NULL) == 0) {
        sram_thread_running = 1;
    } else {
        LOG_ERROR(mux_module, "Failed to start SRAM writer thread, saves will be skipped");
    }
}

void sram_bridge_save(void) {
    if (!sram_path[0] || !current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    const size_t size = current_core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    const void *data = current_core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!data || size == 0) return;

    if (shadow_buf && size == shadow_size && memcmp(data, shadow_buf, size) == 0) return;

    if (!sram_thread_running) return;

    pthread_mutex_lock(&sram_mutex);

    if (size > job_cap) {
        free(job_buf);
        job_buf = malloc(size);
        job_cap = job_buf ? size : 0;
    }

    if (job_buf) {
        memcpy(job_buf, data, size);
        job_size = size;
        snprintf(job_path, sizeof(job_path), "%s", sram_path);
        job_pending = 1;
        pthread_cond_signal(&sram_cond);
    } else {
        LOG_ERROR(mux_module, "Failed to allocate SRAM job buffer (%zu bytes), save skipped", size);
    }

    pthread_mutex_unlock(&sram_mutex);

    if (job_buf) {
        if (size > shadow_cap) {
            free(shadow_buf);
            shadow_buf = malloc(size);
            shadow_cap = shadow_buf ? size : 0;
        }
        if (shadow_buf) {
            memcpy(shadow_buf, data, size);
            shadow_size = size;
        }
    }
}

void sram_bridge_shutdown(void) {
    if (!sram_thread_running) return;

    pthread_mutex_lock(&sram_mutex);
    worker_shutdown = 1;
    pthread_cond_broadcast(&sram_cond);
    while (job_pending || write_in_flight)
        pthread_cond_wait(&sram_cond, &sram_mutex);
    pthread_mutex_unlock(&sram_mutex);

    pthread_join(sram_thread, NULL);
    sram_thread_running = 0;

    free(shadow_buf);
    shadow_buf = NULL;
    shadow_cap = 0;
    shadow_size = 0;

    free(job_buf);
    job_buf = NULL;
    job_cap = 0;

    free(worker_buf);
    worker_buf = NULL;
    worker_cap = 0;
}
