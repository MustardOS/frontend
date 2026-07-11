#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "sram.h"

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

        if (write_size > 0) atomic_write_file(path, worker_buf, write_size);

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

    char core_name[MAX_BUFFER_SIZE];
    core_get_name(core_path_arg, core_name, sizeof(core_name));

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    snprintf(content_stem, sizeof(content_stem), "%s", content_base);
    char *dot = strrchr(content_stem, '.');
    if (dot) *dot = '\0';

    snprintf(sram_path, sizeof(sram_path), "%s/%s/%s.srm", RETRO_SRM_PATH, core_name, content_stem);
    create_directories(sram_path, 1);

    FILE *f = fopen(sram_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        const long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (file_size > 0) {
            const size_t to_read = (size_t) file_size < size ? (size_t) file_size : size;
            if (fread(data, 1, to_read, f) == to_read) {
                LOG_SUCCESS(mux_module, "Loaded SRAM: %s (%zu bytes)", sram_path, to_read);
            } else {
                LOG_ERROR(mux_module, "Failed to read SRAM: %s", sram_path);
            }
        }
        fclose(f);
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
