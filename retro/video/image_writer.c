#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/screenshot.h"
#include "image_writer.h"

static struct {
    pthread_t thread;
    int running;
    pthread_mutex_t mutex;
    pthread_cond_t wake;
    pthread_cond_t idle;
    atomic_int stop;

    int claimed;
    int pending;
    int busy;
    int unavailable;

    uint8_t *pixels;
    size_t capacity;
    int width;
    int height;

    char path[IMAGE_WRITER_PATH_MAX];
    char copies[IMAGE_WRITER_COPY_MAX][IMAGE_WRITER_PATH_MAX];
    unsigned copy_count;
} writer;

static void *image_worker(void *argument) {
    (void) argument;

    for (;;) {
        pthread_mutex_lock(&writer.mutex);
        while (!writer.pending && !atomic_load(&writer.stop))
            pthread_cond_wait(&writer.wake, &writer.mutex);

        if (atomic_load(&writer.stop) && !writer.pending) {
            pthread_mutex_unlock(&writer.mutex);
            break;
        }

        writer.pending = 0;
        writer.busy = 1;

        char path[IMAGE_WRITER_PATH_MAX];
        char copies[IMAGE_WRITER_COPY_MAX][IMAGE_WRITER_PATH_MAX];
        snprintf(path, sizeof(path), "%s", writer.path);
        const unsigned copy_count = writer.copy_count;
        for (unsigned index = 0; index < copy_count; index++)
            snprintf(copies[index], sizeof(copies[index]), "%s", writer.copies[index]);

        uint8_t *const pixels = writer.pixels;
        const int width = writer.width;
        const int height = writer.height;
        pthread_mutex_unlock(&writer.mutex);

        if (screenshot_write_rgb(path, pixels, (uint32_t) width, (uint32_t) height) == 0) {
            for (unsigned index = 0; index < copy_count; index++)
                copy_file(path, copies[index]);
        } else {
            LOG_WARN(mux_module, "image_writer: could not store '%s'", path);
        }

        pthread_mutex_lock(&writer.mutex);
        writer.busy = 0;
        pthread_cond_broadcast(&writer.idle);
        pthread_mutex_unlock(&writer.mutex);
    }

    return NULL;
}

static int worker_start(void) {
    if (writer.running) return 0;
    if (writer.unavailable) return -1;

    pthread_mutex_init(&writer.mutex, NULL);
    pthread_cond_init(&writer.wake, NULL);
    pthread_cond_init(&writer.idle, NULL);
    atomic_store(&writer.stop, 0);

    if (pthread_create(&writer.thread, NULL, image_worker, NULL) != 0) {
        pthread_mutex_destroy(&writer.mutex);
        pthread_cond_destroy(&writer.wake);
        pthread_cond_destroy(&writer.idle);
        writer.unavailable = 1;
        LOG_WARN(mux_module, "image_writer: worker could not start");
        return -1;
    }

    writer.running = 1;
    return 0;
}

int image_writer_available(void) {
    return !writer.unavailable;
}

uint8_t *image_writer_claim(const int width, const int height) {
    if (width <= 0 || height <= 0) return NULL;
    if (worker_start() != 0) return NULL;

    pthread_mutex_lock(&writer.mutex);
    if (writer.claimed || writer.pending || writer.busy) {
        pthread_mutex_unlock(&writer.mutex);
        return NULL;
    }

    const size_t needed = (size_t) width * (size_t) height * 3;
    if (needed > writer.capacity) {
        uint8_t *grown = realloc(writer.pixels, needed);
        if (!grown) {
            pthread_mutex_unlock(&writer.mutex);
            return NULL;
        }
        writer.pixels = grown;
        writer.capacity = needed;
    }

    writer.claimed = 1;
    writer.width = width;
    writer.height = height;
    uint8_t *const pixels = writer.pixels;
    pthread_mutex_unlock(&writer.mutex);

    return pixels;
}

void image_writer_commit(const char *path, const char *const *copies, const unsigned copy_count) {
    if (!path || !*path) {
        image_writer_release();
        return;
    }

    pthread_mutex_lock(&writer.mutex);
    if (!writer.claimed) {
        pthread_mutex_unlock(&writer.mutex);
        return;
    }

    snprintf(writer.path, sizeof(writer.path), "%s", path);
    writer.copy_count = copy_count < IMAGE_WRITER_COPY_MAX ? copy_count : IMAGE_WRITER_COPY_MAX;
    for (unsigned index = 0; index < writer.copy_count; index++)
        snprintf(writer.copies[index], sizeof(writer.copies[index]), "%s", copies[index]);

    writer.claimed = 0;
    writer.pending = 1;
    pthread_cond_signal(&writer.wake);
    pthread_mutex_unlock(&writer.mutex);
}

void image_writer_flush(void) {
    if (!writer.running) return;

    pthread_mutex_lock(&writer.mutex);
    while (writer.pending || writer.busy)
        pthread_cond_wait(&writer.idle, &writer.mutex);
    pthread_mutex_unlock(&writer.mutex);
}

void image_writer_release(void) {
    pthread_mutex_lock(&writer.mutex);
    writer.claimed = 0;
    pthread_mutex_unlock(&writer.mutex);
}

void image_writer_shutdown(void) {
    if (writer.running) {
        atomic_store(&writer.stop, 1);
        pthread_mutex_lock(&writer.mutex);
        pthread_cond_signal(&writer.wake);
        pthread_mutex_unlock(&writer.mutex);
        pthread_join(writer.thread, NULL);
        pthread_mutex_destroy(&writer.mutex);
        pthread_cond_destroy(&writer.wake);
        pthread_cond_destroy(&writer.idle);
        writer.running = 0;
    }

    free(writer.pixels);
    writer.pixels = NULL;
    writer.capacity = 0;
    writer.claimed = 0;
    writer.pending = 0;
    writer.busy = 0;
}
