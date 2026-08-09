#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#include "init.h"
#include "fileio.h"
#include "util.h"
#include "ui/nav.h"
#include "download.h"
#include "log.h"

#define MAX_DOWNLOAD_BYTES ((curl_off_t) (512L * 1024L * 1024L))
#define TEMP_NAME_ATTEMPTS 64

_Atomic int cancel_download = 0;
_Atomic int download_in_progress = 0;

typedef enum { download_idle, download_running, download_completed } download_state;

typedef struct {
    pthread_mutex_t mutex;
    download_state state;
    int result;
    void (*configured_cb)(int);
    void (*completion_cb)(int);
} download_control;

typedef struct {
    char *url;
    char *save_path;
} download_args_t;

typedef struct {
    int directory_fd;
    int file_fd;
    FILE *stream;
    char *directory;
    char *basename;
    char temporary_name[64];
} download_target;

static download_control control = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .state = download_idle,
};

static size_t write_data(const void *ptr, const size_t size, const size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream) * size;
}

static int fill_random(void *buffer, const size_t size) {
    unsigned char *out = buffer;
    size_t offset = 0;

    while (offset < size) {
        const ssize_t got = getrandom(out + offset, size - offset, 0);
        if (got > 0) {
            offset += (size_t) got;
            continue;
        }
        if (got < 0 && errno == EINTR) continue;
        return -1;
    }

    return 0;
}

static void close_target(download_target *target, const int remove_temporary) {
    if (!target) return;

    if (target->stream) {
        fclose(target->stream);
        target->stream = NULL;
        target->file_fd = -1;
    } else if (target->file_fd >= 0) {
        close(target->file_fd);
        target->file_fd = -1;
    }

    if (remove_temporary && target->directory_fd >= 0 && target->temporary_name[0])
        unlinkat(target->directory_fd, target->temporary_name, 0);

    if (target->directory_fd >= 0) close(target->directory_fd);
    free(target->directory);
    free(target->basename);

    target->directory_fd = -1;
    target->temporary_name[0] = '\0';
}

static int split_output_path(const char *path, char **directory, char **basename) {
    if (!path || !*path) return -1;

    const char *slash = strrchr(path, '/');
    if (!slash) {
        *directory = mux_strdup(".");
        *basename = mux_strdup(path);
    } else {
        const size_t directory_len = slash == path ? 1 : (size_t) (slash - path);
        *directory = mux_malloc(directory_len + 1);
        memcpy(*directory, path, directory_len);
        (*directory)[directory_len] = '\0';
        *basename = mux_strdup(slash + 1);
    }

    if (!**basename || strcmp(*basename, ".") == 0 || strcmp(*basename, "..") == 0 || strchr(*basename, '/')) {
        free(*directory);
        free(*basename);
        *directory = NULL;
        *basename = NULL;
        return -1;
    }

    return 0;
}

static int open_download_target(const char *output_path, download_target *target) {
    memset(target, 0, sizeof(*target));
    target->directory_fd = -1;
    target->file_fd = -1;

    create_directories(output_path, 1);
    if (split_output_path(output_path, &target->directory, &target->basename) != 0) return -1;

    target->directory_fd = open(target->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (target->directory_fd < 0) {
        close_target(target, 0);
        return -1;
    }

    for (int attempt = 0; attempt < TEMP_NAME_ATTEMPTS; attempt++) {
        uint64_t random_value[2];
        if (fill_random(random_value, sizeof(random_value)) != 0) {
            close_target(target, 0);
            return -1;
        }

        snprintf(
            target->temporary_name, sizeof(target->temporary_name), ".muos-download-%016llx%016llx.part",
            (unsigned long long) random_value[0], (unsigned long long) random_value[1]
        );

        target->file_fd = openat(
            target->directory_fd, target->temporary_name, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600
        );
        if (target->file_fd >= 0) break;
        if (errno != EEXIST) {
            close_target(target, 0);
            return -1;
        }
    }

    if (target->file_fd < 0) {
        close_target(target, 0);
        return -1;
    }

    target->stream = fdopen(target->file_fd, "wb");
    if (!target->stream) {
        close_target(target, 1);
        return -1;
    }

    return 0;
}

static int publish_download(download_target *target) {
    if (fflush(target->stream) != 0 || fsync(target->file_fd) != 0) return -1;
    if (fclose(target->stream) != 0) {
        target->stream = NULL;
        target->file_fd = -1;
        return -1;
    }

    target->stream = NULL;
    target->file_fd = -1;

    if (renameat(target->directory_fd, target->temporary_name, target->directory_fd, target->basename) != 0) return -1;

    target->temporary_name[0] = '\0';
    if (fsync(target->directory_fd) == 0) return 0;

    if (errno == EINVAL || errno == EROFS || errno == EOPNOTSUPP) {
        LOG_WARN(mux_module, "Directory durability is not supported for downloaded file: %s", target->directory);
        return 0;
    }

    return -1;
}

void set_download_callbacks(void (*callback)(int)) {
    pthread_mutex_lock(&control.mutex);
    if (control.state == download_idle) control.configured_cb = callback;
    pthread_mutex_unlock(&control.mutex);
}

void download_poll(void) {
    pthread_mutex_lock(&control.mutex);
    if (control.state != download_completed) {
        pthread_mutex_unlock(&control.mutex);
        return;
    }

    const int result = control.result;
    void (*cb)(int) = control.completion_cb;
    control.result = 0;
    control.completion_cb = NULL;
    control.state = download_idle;
    atomic_store_explicit(&download_in_progress, 0, memory_order_release);
    atomic_store_explicit(&cancel_download, 0, memory_order_release);
    pthread_mutex_unlock(&control.mutex);

    if (result == 0) atomic_store_explicit(&progress_bar_value, 100, memory_order_relaxed);
    hide_progress_bar();
    if (cb) cb(result);
}

static int progress_callback(
    void *clientp, const curl_off_t dltotal, const curl_off_t dlnow, const curl_off_t ultotal, const curl_off_t ulnow
) {
    (void) clientp;
    (void) ultotal;
    (void) ulnow;

    if (atomic_load_explicit(&cancel_download, memory_order_acquire)) {
        LOG_INFO(mux_module, "Cancelling download");
        return 1;
    }

    if (dlnow > MAX_DOWNLOAD_BYTES || dltotal > MAX_DOWNLOAD_BYTES) return 1;

    if (dltotal > 0) {
        const int percent = (int) ((dlnow * 100) / dltotal);
        atomic_store_explicit(&progress_bar_value, percent, memory_order_relaxed);
    } else if (dlnow > 0) {
        atomic_store_explicit(&progress_bar_value, PROGRESS_INDETERMINATE, memory_order_relaxed);
    }

    return 0;
}

static int perform_download(const char *url, const char *output_path) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    download_target target;
    if (open_download_target(output_path, &target) != 0) {
        curl_easy_cleanup(curl);
        return -2;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, target.stream);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 300000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, MAX_DOWNLOAD_BYTES);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    const CURLcode result = curl_easy_perform(curl);
    long response_code = 0;
    curl_off_t downloaded = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    const CURLcode size_result = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloaded);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        LOG_ERROR(mux_module, "cURL failure: %s", curl_easy_strerror(result));
        close_target(&target, 1);
        return -3;
    }
    if (response_code < 200 || response_code >= 300) {
        LOG_ERROR(mux_module, "Unexpected HTTP status: %ld", response_code);
        close_target(&target, 1);
        return -4;
    }
    if (size_result != CURLE_OK || downloaded <= 0 || downloaded > MAX_DOWNLOAD_BYTES) {
        LOG_ERROR(mux_module, "Downloaded file has an invalid size");
        close_target(&target, 1);
        return -5;
    }
    if (publish_download(&target) != 0) {
        LOG_ERROR(mux_module, "Failed to publish completed download: %s", strerror(errno));
        close_target(&target, 1);
        return -6;
    }

    close_target(&target, 0);
    LOG_SUCCESS(mux_module, "Download finished (%lld bytes)", (long long) downloaded);
    return 0;
}

static void complete_download(const int result) {
    pthread_mutex_lock(&control.mutex);
    control.result = result;
    control.state = download_completed;
    pthread_mutex_unlock(&control.mutex);
}

static void *download_thread(void *arg) {
    download_args_t *args = arg;
    const int result = perform_download(args->url, args->save_path);

    free(args->url);
    free(args->save_path);
    free(args);

    complete_download(result);
    return NULL;
}

static int schedule_start_failure_locked(const int result) {
    if (control.state == download_idle) {
        control.completion_cb = control.configured_cb;
        control.configured_cb = NULL;
    }
    control.result = result;
    control.state = download_completed;
    atomic_store_explicit(&download_in_progress, 0, memory_order_release);
    return 0;
}

int initiate_download(const char *url, const char *output_path, const int show_progress, char *message) {
    pthread_mutex_lock(&control.mutex);
    if (control.state != download_idle) {
        pthread_mutex_unlock(&control.mutex);
        return -1;
    }

    if (!url || !*url || !output_path || !*output_path) {
        const int result = schedule_start_failure_locked(-1);
        pthread_mutex_unlock(&control.mutex);
        return result;
    }

    download_args_t *args = calloc(1, sizeof(*args));
    if (!args) {
        const int result = schedule_start_failure_locked(-2);
        pthread_mutex_unlock(&control.mutex);
        return result;
    }

    args->url = strdup(url);
    args->save_path = strdup(output_path);
    if (!args->url || !args->save_path) {
        free(args->url);
        free(args->save_path);
        free(args);
        const int result = schedule_start_failure_locked(-2);
        pthread_mutex_unlock(&control.mutex);
        return result;
    }

    control.completion_cb = control.configured_cb;
    control.configured_cb = NULL;
    control.result = 0;
    control.state = download_running;
    atomic_store_explicit(&cancel_download, 0, memory_order_release);
    atomic_store_explicit(&download_in_progress, 1, memory_order_release);
    atomic_store_explicit(&progress_bar_value, 0, memory_order_relaxed);

    pthread_t thread;
    const int thread_result = pthread_create(&thread, NULL, download_thread, args);
    if (thread_result != 0) {
        free(args->url);
        free(args->save_path);
        free(args);
        schedule_start_failure_locked(-7);
        pthread_mutex_unlock(&control.mutex);
        return 0;
    }

    pthread_detach(thread);
    pthread_mutex_unlock(&control.mutex);

    if (show_progress) show_progress_bar(message);
    return 0;
}
