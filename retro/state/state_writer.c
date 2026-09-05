#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "state_format.h"
#include "state_writer.h"

struct state_write_job {
    char path[MAX_BUFFER_SIZE];
    uint8_t *data;
    size_t size;
    size_t core_size;
    size_t cheevo_size;
};

static pthread_t writer_thread;
static pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t writer_wake = PTHREAD_COND_INITIALIZER;
static pthread_cond_t writer_idle = PTHREAD_COND_INITIALIZER;
static struct state_write_job writer_job;
static int writer_started;
static int writer_busy;
static int writer_stop;
static int writer_result;

static int atomic_write(const char *path, const void *data, const size_t size) {
    char temporary[MAX_BUFFER_SIZE];
    const int path_length = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long) getpid());
    if (path_length < 0 || (size_t) path_length >= sizeof(temporary)) {
        LOG_ERROR(mux_module, "Save-state path is too long: '%s'", path);
        return -1;
    }

    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644);
    if (descriptor < 0 && errno == EEXIST) {
        unlink(temporary);
        descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644);
    }
    FILE *file = descriptor >= 0 ? fdopen(descriptor, "wb") : NULL;
    if (!file) {
        if (descriptor >= 0) close(descriptor);
        LOG_ERROR(mux_module, "Failed to open '%s' for save state", temporary);
        return -1;
    }

    int okay = fwrite(data, 1, size, file) == size;
    if (okay && fflush(file) != 0) okay = 0;
    if (okay && fdatasync(fileno(file)) != 0) okay = 0;
    if (fclose(file) != 0) okay = 0;

    if (!okay) {
        LOG_ERROR(mux_module, "Short write saving state to '%s'", temporary);
        remove(temporary);
        return -1;
    }

    if (rename(temporary, path) != 0) {
        LOG_ERROR(mux_module, "Failed to rename '%s' to '%s'", temporary, path);
        remove(temporary);
        return -1;
    }

    char directory[MAX_BUFFER_SIZE];
    snprintf(directory, sizeof(directory), "%s", path);
    char *slash = strrchr(directory, '/');
    if (slash) {
        *slash = '\0';
    } else {
        snprintf(directory, sizeof(directory), ".");
    }

    const int directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory_fd < 0) {
        LOG_ERROR(mux_module, "Failed to open save-state directory '%s'", directory);
        return -1;
    }
    const int sync_result = fsync(directory_fd);
    const int sync_error = errno;
    close(directory_fd);
    if (sync_result != 0 && sync_error != EINVAL && sync_error != EROFS) {
        LOG_ERROR(mux_module, "Failed to synchronise save-state directory '%s'", directory);
        return -1;
    }
    return 0;
}

static void *writer_main(void *unused) {
    (void) unused;
    for (;;) {
        pthread_mutex_lock(&writer_mutex);
        while (!writer_job.data && !writer_stop)
            pthread_cond_wait(&writer_wake, &writer_mutex);
        if (writer_stop && !writer_job.data) {
            pthread_mutex_unlock(&writer_mutex);
            break;
        }

        struct state_write_job job = writer_job;
        memset(&writer_job, 0, sizeof(writer_job));
        writer_busy = 1;
        pthread_mutex_unlock(&writer_mutex);

        state_write_u32(job.data + 16, crc32(0, job.data + STATE_HEADER_SIZE, (uInt) job.core_size));
        state_write_u32(
            job.data + 20,
            job.cheevo_size ? crc32(0, job.data + STATE_HEADER_SIZE + job.core_size, (uInt) job.cheevo_size) : 0
        );
        const int result = atomic_write(job.path, job.data, job.size);
        if (result == 0)
            LOG_SUCCESS(
                mux_module, "Saved state to '%s' (%zu core bytes, %zu achievement bytes)", job.path, job.core_size,
                job.cheevo_size
            );
        free(job.data);

        pthread_mutex_lock(&writer_mutex);
        writer_result = result;
        writer_busy = 0;
        pthread_cond_broadcast(&writer_idle);
        pthread_mutex_unlock(&writer_mutex);
    }
    return NULL;
}

static int start_writer(void) {
    pthread_mutex_lock(&writer_mutex);
    if (writer_started) {
        pthread_mutex_unlock(&writer_mutex);
        return 0;
    }
    writer_stop = 0;
    writer_result = 0;
    if (pthread_create(&writer_thread, NULL, writer_main, NULL) != 0) {
        pthread_mutex_unlock(&writer_mutex);
        LOG_ERROR(mux_module, "Could not start the save-state persistence worker");
        return -1;
    }
    writer_started = 1;
    pthread_mutex_unlock(&writer_mutex);
    return 0;
}

int state_writer_wait(void) {
    pthread_mutex_lock(&writer_mutex);
    while (writer_job.data || writer_busy)
        pthread_cond_wait(&writer_idle, &writer_mutex);
    const int result = writer_result;
    writer_result = 0;
    pthread_mutex_unlock(&writer_mutex);
    return result;
}

int state_writer_busy(void) {
    pthread_mutex_lock(&writer_mutex);
    const int busy = writer_job.data != NULL || writer_busy;
    pthread_mutex_unlock(&writer_mutex);
    return busy;
}

int state_writer_submit(
    const char *path, uint8_t *data, const size_t size, const size_t core_size, const size_t cheevo_size
) {
    if (!path || !data || size < STATE_HEADER_SIZE) return -1;
    if (strlen(path) >= sizeof(writer_job.path)) return -1;
    if (start_writer() != 0) return -1;

    pthread_mutex_lock(&writer_mutex);
    while (writer_job.data || writer_busy)
        pthread_cond_wait(&writer_idle, &writer_mutex);
    snprintf(writer_job.path, sizeof(writer_job.path), "%s", path);
    writer_job.data = data;
    writer_job.size = size;
    writer_job.core_size = core_size;
    writer_job.cheevo_size = cheevo_size;
    writer_result = 0;
    pthread_cond_signal(&writer_wake);
    pthread_mutex_unlock(&writer_mutex);
    return 0;
}

int state_writer_flush(void) {
    return state_writer_wait();
}

void state_writer_shutdown(void) {
    pthread_mutex_lock(&writer_mutex);
    if (!writer_started) {
        pthread_mutex_unlock(&writer_mutex);
        return;
    }
    while (writer_job.data || writer_busy)
        pthread_cond_wait(&writer_idle, &writer_mutex);
    writer_stop = 1;
    pthread_cond_signal(&writer_wake);
    pthread_mutex_unlock(&writer_mutex);
    pthread_join(writer_thread, NULL);

    pthread_mutex_lock(&writer_mutex);
    writer_started = 0;
    writer_stop = 0;
    writer_result = 0;
    pthread_mutex_unlock(&writer_mutex);
}
