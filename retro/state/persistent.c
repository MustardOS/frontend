#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/miniz/miniz.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "persistent.h"

#define PERSISTENT_BACKUP_MAX      10
#define PERSISTENT_FILE_SIZE_LIMIT (64U * 1024U * 1024U)

typedef struct {
    unsigned memory_id;
    const char *extension;
    const char *name;
    char path[MAX_BUFFER_SIZE];
    uint8_t *shadow;
    size_t shadow_capacity;
    size_t shadow_size;
    uint8_t *preserved;
    size_t preserved_capacity;
    size_t preserved_size;
    int active;
} persistent_region;

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t size;
    size_t core_size;
    int pending;
} persistent_snapshot;

enum { persistent_save_ram, persistent_rtc, persistent_region_count };

static persistent_region regions[persistent_region_count] = {
    [persistent_save_ram] = {.memory_id = RETRO_MEMORY_SAVE_RAM, .extension = "srm", .name = "save RAM"},
    [persistent_rtc] = {.memory_id = RETRO_MEMORY_RTC, .extension = "rtc", .name = "RTC"},
};

static persistent_snapshot queued[persistent_region_count];
static persistent_snapshot writing[persistent_region_count];

static pthread_mutex_t persistence_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t persistence_wake = PTHREAD_COND_INITIALIZER;
static pthread_t persistence_thread;

static int persistence_thread_running = 0;
static int worker_shutdown = 0;
static int write_in_flight = 0;
static int write_failed = 0;

static int resize_buffer(uint8_t **buffer, size_t *capacity, const size_t required) {
    if (required <= *capacity) return 1;

    uint8_t *resized = realloc(*buffer, required);
    if (!resized) return 0;

    *buffer = resized;
    *capacity = required;
    return 1;
}

static int parent_directory(const char *path, char *directory, const size_t directory_size) {
    if (!str_copy_checked(directory, directory_size, path)) return 0;

    char *slash = strrchr(directory, '/');
    if (!slash) return 0;
    if (slash == directory)
        slash[1] = '\0';
    else
        *slash = '\0';
    return 1;
}

static void synchronise_parent_directory(const char *path) {
    char directory[MAX_BUFFER_SIZE];
    if (!parent_directory(path, directory, sizeof(directory))) return;

    const int descriptor = open(directory, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return;
    if (fsync(descriptor) != 0 && errno != EINVAL && errno != EROFS)
        LOG_WARN(mux_module, "Could not synchronise persistent-memory directory '%s': %s", directory, strerror(errno));
    close(descriptor);
}

static int atomic_write_file(const char *path, const void *data, const size_t size) {
    char temporary[MAX_BUFFER_SIZE];
    if (!str_format_checked(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long) getpid())) {
        LOG_ERROR(mux_module, "Persistent-memory temporary path is too long: %s", path);
        return -1;
    }

    int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0 && errno == EEXIST) {
        unlink(temporary);
        descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    }
    if (descriptor < 0) {
        LOG_ERROR(mux_module, "Could not open persistent-memory temporary file '%s': %s", temporary, strerror(errno));
        return -1;
    }

    FILE *file = fdopen(descriptor, "wb");
    if (!file) {
        LOG_ERROR(mux_module, "Could not stream persistent-memory file '%s': %s", temporary, strerror(errno));
        close(descriptor);
        unlink(temporary);
        return -1;
    }

    int okay = size == 0 || fwrite(data, 1, size, file) == size;
    if (okay && fflush(file) != 0) okay = 0;
    if (okay && fdatasync(descriptor) != 0) okay = 0;
    if (fclose(file) != 0) okay = 0;

    if (!okay) {
        LOG_ERROR(mux_module, "Could not safely write persistent-memory file '%s': %s", temporary, strerror(errno));
        unlink(temporary);
        return -1;
    }

    if (rename(temporary, path) != 0) {
        LOG_ERROR(mux_module, "Could not publish persistent-memory file '%s': %s", path, strerror(errno));
        unlink(temporary);
        return -1;
    }

    synchronise_parent_directory(path);

    return 0;
}

static int checksum_path(const char *path, char *output, const size_t output_size) {
    return str_format_checked(output, output_size, "%s.sum", path);
}

static uint32_t compute_checksum(const void *data, const size_t size) {
    return (uint32_t) mz_crc32(MZ_CRC32_INIT, data, size);
}

static int checksum_text(const char *path, const void *data, const size_t size, char output[128], size_t *output_size) {
    struct stat status;
    if (stat(path, &status) != 0) return 0;

    const int length = snprintf(
        output, 128, "%08X %zu %lld %ld %llu", compute_checksum(data, size), size,
        (long long) status.st_mtim.tv_sec, status.st_mtim.tv_nsec, (unsigned long long) status.st_ino
    );
    if (length <= 0 || length >= 128) return 0;
    *output_size = (size_t) length;
    return 1;
}

static int write_checksum(const char *path, const void *data, const size_t size) {
    char path_sum[MAX_BUFFER_SIZE];
    if (!checksum_path(path, path_sum, sizeof(path_sum))) return -1;

    char checksum[128];
    size_t checksum_size = 0;
    if (!checksum_text(path, data, size, checksum, &checksum_size)) return -1;
    return atomic_write_file(path_sum, checksum, checksum_size);
}

enum { checksum_invalid = 0, checksum_valid = 1, checksum_external_update = 2 };

static int checksum_matches(const char *path, const void *data, const size_t size, const int allow_external_update) {
    char path_sum[MAX_BUFFER_SIZE];
    if (!checksum_path(path, path_sum, sizeof(path_sum))) return 0;

    FILE *file = fopen(path_sum, "r");
    if (!file) return errno == ENOENT ? checksum_valid : checksum_invalid;

    char line[160] = "";
    const int read = fgets(line, sizeof(line), file) != NULL;
    fclose(file);
    if (!read) return checksum_invalid;

    char stored[16] = "";
    size_t stored_size = 0;
    long long stored_seconds = 0;
    long stored_nanoseconds = 0;
    unsigned long long stored_inode = 0;
    const int fields = sscanf(
        line, "%15s %zu %lld %ld %llu", stored, &stored_size, &stored_seconds, &stored_nanoseconds, &stored_inode
    );
    if (fields < 1) return checksum_invalid;

    char expected[16];
    snprintf(expected, sizeof(expected), "%08X", compute_checksum(data, size));
    if (strcasecmp(stored, expected) == 0) return checksum_valid;
    if (!allow_external_update) return checksum_invalid;

    struct stat data_status;
    struct stat sum_status;
    if (stat(path, &data_status) != 0 || stat(path_sum, &sum_status) != 0) return checksum_invalid;

    if (fields == 5) {
        if (stored_size != size || stored_seconds != (long long) data_status.st_mtim.tv_sec
            || stored_nanoseconds != data_status.st_mtim.tv_nsec || stored_inode != (unsigned long long) data_status.st_ino)
            return checksum_external_update;
    } else if (data_status.st_mtim.tv_sec > sum_status.st_mtim.tv_sec
               || (data_status.st_mtim.tv_sec == sum_status.st_mtim.tv_sec
                   && data_status.st_mtim.tv_nsec > sum_status.st_mtim.tv_nsec)) {
        // Compatibility with checksum files written by earlier Pickles builds.
        return checksum_external_update;
    }

    return checksum_invalid;
}

static int read_persistent_file(const char *path, uint8_t **output, size_t *output_size) {
    *output = NULL;
    *output_size = 0;

    FILE *file = fopen(path, "rb");
    if (!file) return errno == ENOENT ? 0 : -1;

    int result = -1;
    if (fseek(file, 0, SEEK_END) != 0) goto finish;

    const long length = ftell(file);
    if (length <= 0 || (unsigned long) length > PERSISTENT_FILE_SIZE_LIMIT) goto finish;
    if (fseek(file, 0, SEEK_SET) != 0) goto finish;

    uint8_t *data = malloc((size_t) length);
    if (!data) goto finish;

    if (fread(data, 1, (size_t) length, file) != (size_t) length) {
        free(data);
        goto finish;
    }

    *output = data;
    *output_size = (size_t) length;
    result = 1;

finish:
    fclose(file);
    return result;
}

static void rotate_backups(const char *path) {
    char oldest[MAX_BUFFER_SIZE];
    char oldest_sum[MAX_BUFFER_SIZE];
    if (!str_format_checked(oldest, sizeof(oldest), "%s.bk%d", path, PERSISTENT_BACKUP_MAX - 1)
        || !str_format_checked(oldest_sum, sizeof(oldest_sum), "%s.bk%d.sum", path, PERSISTENT_BACKUP_MAX - 1))
        return;
    unlink(oldest);
    unlink(oldest_sum);

    for (int index = PERSISTENT_BACKUP_MAX - 2; index >= 0; index--) {
        char source[MAX_BUFFER_SIZE];
        char destination[MAX_BUFFER_SIZE];
        char source_sum[MAX_BUFFER_SIZE];
        char destination_sum[MAX_BUFFER_SIZE];
        if (!str_format_checked(source, sizeof(source), "%s.bk%d", path, index)
            || !str_format_checked(destination, sizeof(destination), "%s.bk%d", path, index + 1)
            || !str_format_checked(source_sum, sizeof(source_sum), "%s.bk%d.sum", path, index)
            || !str_format_checked(destination_sum, sizeof(destination_sum), "%s.bk%d.sum", path, index + 1))
            return;
        rename(source, destination);
        rename(source_sum, destination_sum);
    }

    if (!file_exist(path)) return;

    char newest[MAX_BUFFER_SIZE];
    char newest_sum[MAX_BUFFER_SIZE];
    char path_sum[MAX_BUFFER_SIZE];
    if (!str_format_checked(newest, sizeof(newest), "%s.bk0", path)
        || !str_format_checked(newest_sum, sizeof(newest_sum), "%s.bk0.sum", path)
        || !checksum_path(path, path_sum, sizeof(path_sum)))
        return;
    rename(path, newest);
    rename(path_sum, newest_sum);
}

static void restore_rotated_primary(const char *path) {
    if (access(path, F_OK) == 0) return;

    char newest[MAX_BUFFER_SIZE];
    char newest_sum[MAX_BUFFER_SIZE];
    char path_sum[MAX_BUFFER_SIZE];
    if (!str_format_checked(newest, sizeof(newest), "%s.bk0", path)
        || !str_format_checked(newest_sum, sizeof(newest_sum), "%s.bk0.sum", path)
        || !checksum_path(path, path_sum, sizeof(path_sum)))
        return;

    if (access(newest, F_OK) != 0) return;

    if (rename(newest, path) != 0) {
        LOG_ERROR(mux_module, "Could not restore the previous persistent-memory file '%s': %s", path, strerror(errno));
        return;
    }

    if (access(path_sum, F_OK) != 0 && access(newest_sum, F_OK) == 0 && rename(newest_sum, path_sum) != 0)
        LOG_WARN(mux_module, "Could not restore the previous checksum '%s': %s", path_sum, strerror(errno));
    synchronise_parent_directory(path);
}

static int publish_snapshot(const persistent_region *region, const persistent_snapshot *snapshot) {
    char staged[MAX_BUFFER_SIZE];
    char staged_sum[MAX_BUFFER_SIZE];
    char path_sum[MAX_BUFFER_SIZE];
    if (!str_format_checked(staged, sizeof(staged), "%s.pending.%ld", region->path, (long) getpid())
        || !str_format_checked(staged_sum, sizeof(staged_sum), "%s.sum.pending.%ld", region->path, (long) getpid())
        || !checksum_path(region->path, path_sum, sizeof(path_sum))) {
        LOG_ERROR(mux_module, "%s staging path is too long: %s", region->name, region->path);
        return -1;
    }

    char sum[128];
    size_t sum_size = 0;
    if (atomic_write_file(staged, snapshot->data, snapshot->size) != 0
        || !checksum_text(staged, snapshot->data, snapshot->size, sum, &sum_size)
        || atomic_write_file(staged_sum, sum, sum_size) != 0) {
        unlink(staged);
        unlink(staged_sum);
        return -1;
    }

    // Only move the previous primary into the recovery chain after the complete
    // replacement and its checksum are safely staged on the same filesystem.
    rotate_backups(region->path);
    if (rename(staged, region->path) != 0) {
        LOG_ERROR(mux_module, "Could not publish %s '%s': %s", region->name, region->path, strerror(errno));
        restore_rotated_primary(region->path);
        unlink(staged);
        unlink(staged_sum);
        return -1;
    }
    if (rename(staged_sum, path_sum) != 0) {
        // The raw save is already durable and remains compatible with imported
        // RetroArch files. Do not let a stale sidecar invalidate it on launch.
        unlink(path_sum);
        unlink(staged_sum);
        LOG_WARN(mux_module, "Saved %s without a checksum sidecar: %s", region->name, region->path);
    }
    synchronise_parent_directory(region->path);

    LOG_SUCCESS(mux_module, "Saved %s: %s (%zu bytes)", region->name, region->path, snapshot->size);
    return 0;
}

static int load_candidate(
    persistent_region *region, const char *path, void *core_data, const size_t core_size, const int allow_external_update
) {
    uint8_t *file_data = NULL;
    size_t file_size = 0;
    const int read = read_persistent_file(path, &file_data, &file_size);
    if (read <= 0) return read;

    const int checksum = checksum_matches(path, file_data, file_size, allow_external_update);
    if (checksum == checksum_invalid) {
        LOG_ERROR(mux_module, "%s failed validation: %s", region->name, path);
        free(file_data);
        return -1;
    }

    const size_t copy_size = file_size < core_size ? file_size : core_size;
    memcpy(core_data, file_data, copy_size);

    if (!resize_buffer(&region->preserved, &region->preserved_capacity, file_size)) {
        LOG_ERROR(mux_module, "Could not preserve the complete %s file (%zu bytes)", region->name, file_size);
        free(file_data);
        return -1;
    }
    memcpy(region->preserved, file_data, file_size);
    region->preserved_size = file_size;

    if (checksum == checksum_external_update) {
        LOG_INFO(mux_module, "Accepting %s updated by another frontend: %s", region->name, path);
        if (write_checksum(path, file_data, file_size) != 0)
            LOG_WARN(mux_module, "Could not refresh the %s checksum: %s", region->name, path);
    }
    free(file_data);

    if (file_size > core_size)
        LOG_WARN(
            mux_module, "%s file is larger than the core currently reports; preserving %zu trailing bytes: %s",
            region->name, file_size - core_size, path
        );

    LOG_SUCCESS(mux_module, "Loaded %s: %s (%zu bytes)", region->name, path, copy_size);
    return 1;
}

static int load_region(persistent_region *region, void *core_data, const size_t core_size) {
    int loaded = load_candidate(region, region->path, core_data, core_size, 1);
    if (loaded == 1) return 1;

    for (int index = 0; index < PERSISTENT_BACKUP_MAX; index++) {
        char backup[MAX_BUFFER_SIZE];
        if (!str_format_checked(backup, sizeof(backup), "%s.bk%d", region->path, index)) continue;

        loaded = load_candidate(region, backup, core_data, core_size, 0);
        if (loaded != 1) continue;

        LOG_WARN(mux_module, "Recovered %s from backup: %s", region->name, backup);
        if (atomic_write_file(region->path, region->preserved, region->preserved_size) == 0)
            write_checksum(region->path, region->preserved, region->preserved_size);
        return 1;
    }

    if (file_exist(region->path)) LOG_ERROR(mux_module, "No valid %s or backup was found: %s", region->name, region->path);
    return 0;
}

static int snapshots_pending(void) {
    for (int index = 0; index < persistent_region_count; index++)
        if (queued[index].pending) return 1;
    return 0;
}

static void record_successful_write(const int index, const persistent_snapshot *snapshot) {
    persistent_region *region = &regions[index];

    if (!resize_buffer(&region->shadow, &region->shadow_capacity, snapshot->core_size)) {
        LOG_WARN(mux_module, "Could not retain the %s write shadow; the next flush will write it again", region->name);
        region->shadow_size = 0;
        return;
    }

    memcpy(region->shadow, snapshot->data, snapshot->core_size);
    region->shadow_size = snapshot->core_size;
}

static void *persistence_worker(void *argument) {
    (void) argument;
    pthread_mutex_lock(&persistence_mutex);

    for (;;) {
        while (!snapshots_pending() && !worker_shutdown)
            pthread_cond_wait(&persistence_wake, &persistence_mutex);
        if (!snapshots_pending() && worker_shutdown) break;

        int writing_count = 0;
        for (int index = 0; index < persistent_region_count; index++) {
            persistent_snapshot *source = &queued[index];
            persistent_snapshot *destination = &writing[index];
            if (!source->pending) continue;

            if (!resize_buffer(&destination->data, &destination->capacity, source->size)) {
                LOG_ERROR(mux_module, "Could not allocate the %s writer buffer (%zu bytes)", regions[index].name, source->size);
                source->pending = 0;
                write_failed = 1;
                continue;
            }

            memcpy(destination->data, source->data, source->size);
            destination->size = source->size;
            destination->core_size = source->core_size;
            destination->pending = 1;
            source->pending = 0;
            writing_count++;
        }

        write_in_flight = writing_count > 0;
        pthread_mutex_unlock(&persistence_mutex);

        int results[persistent_region_count] = {0};
        for (int index = 0; index < persistent_region_count; index++) {
            if (!writing[index].pending) continue;
            results[index] = publish_snapshot(&regions[index], &writing[index]);
        }

        pthread_mutex_lock(&persistence_mutex);
        for (int index = 0; index < persistent_region_count; index++) {
            if (!writing[index].pending) continue;
            if (results[index] == 0)
                record_successful_write(index, &writing[index]);
            else
                write_failed = 1;
            writing[index].pending = 0;
        }
        write_in_flight = 0;
        pthread_cond_broadcast(&persistence_wake);
    }

    pthread_mutex_unlock(&persistence_mutex);
    return NULL;
}

static int queue_region(persistent_region *region, persistent_snapshot *snapshot) {
    const size_t core_size = current_core.retro_get_memory_size(region->memory_id);
    const void *core_data = current_core.retro_get_memory_data(region->memory_id);
    if (!core_data || core_size == 0) return 0;

    if (region->memory_id == RETRO_MEMORY_SAVE_RAM) core_cache_save_memory_size(core_size);
    if (region->shadow && region->shadow_size == core_size && memcmp(core_data, region->shadow, core_size) == 0)
        return 0;

    const size_t write_size = core_size > region->preserved_size ? core_size : region->preserved_size;
    if (!resize_buffer(&snapshot->data, &snapshot->capacity, write_size)
        || !resize_buffer(&region->preserved, &region->preserved_capacity, write_size)) {
        LOG_ERROR(mux_module, "Could not capture %s (%zu bytes); it has not been saved", region->name, write_size);
        write_failed = 1;
        return -1;
    }

    if (region->preserved_size > 0) memcpy(snapshot->data, region->preserved, region->preserved_size);
    memcpy(snapshot->data, core_data, core_size);
    memcpy(region->preserved, snapshot->data, write_size);
    region->preserved_size = write_size;

    snapshot->size = write_size;
    snapshot->core_size = core_size;
    snapshot->pending = 1;
    return 1;
}

static void write_queued_synchronously(void) {
    for (int index = 0; index < persistent_region_count; index++) {
        persistent_snapshot *snapshot = &queued[index];
        if (!snapshot->pending) continue;

        if (publish_snapshot(&regions[index], snapshot) == 0)
            record_successful_write(index, snapshot);
        else
            write_failed = 1;
        snapshot->pending = 0;
    }
}

void persistent_memory_init(const char *core_path, const char *content_path) {
    core_cache_save_memory_size(0);
    worker_shutdown = 0;
    write_in_flight = 0;
    write_failed = 0;

    if (!current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    const char *content_base = strrchr(content_path, '/');
    content_base = content_base ? content_base + 1 : content_path;

    char content_stem[MAX_BUFFER_SIZE];
    if (!str_copy_checked(content_stem, sizeof(content_stem), content_base)) {
        LOG_ERROR(mux_module, "Persistent-memory content name is too long");
        return;
    }
    char *extension = strrchr(content_stem, '.');
    if (extension) *extension = '\0';

    char save_prefix[MAX_BUFFER_SIZE];
    if (!core_content_save_prefix(core_path, content_path, save_prefix, sizeof(save_prefix))) {
        LOG_ERROR(mux_module, "Persistent-memory save prefix is too long");
        return;
    }

    char save_directory[MAX_BUFFER_SIZE];
    const char *parts[] = {RETRO_SRM_PATH, save_prefix};
    if (!path_join_checked(save_directory, sizeof(save_directory), parts, A_SIZE(parts))) {
        LOG_ERROR(mux_module, "Persistent-memory directory is too long");
        return;
    }

    int active_regions = 0;
    for (int index = 0; index < persistent_region_count; index++) {
        persistent_region *region = &regions[index];
        region->path[0] = '\0';
        region->active = 0;

        const size_t size = current_core.retro_get_memory_size(region->memory_id);
        void *data = current_core.retro_get_memory_data(region->memory_id);
        if (!data || size == 0) continue;

        if (!str_format_checked(
                region->path, sizeof(region->path), "%s/%s.%s", save_directory, content_stem, region->extension
            )) {
            LOG_ERROR(mux_module, "%s path is too long", region->name);
            continue;
        }
        create_directories(region->path, 1);

        region->active = 1;
        active_regions++;
        if (region->memory_id == RETRO_MEMORY_SAVE_RAM) core_cache_save_memory_size(size);

        const int loaded = load_region(region, data, size);
        if (loaded == 1 && resize_buffer(&region->shadow, &region->shadow_capacity, size)) {
            memcpy(region->shadow, data, size);
            region->shadow_size = size;
        } else {
            region->shadow_size = 0;
        }
    }

    if (active_regions == 0) return;

    if (pthread_create(&persistence_thread, NULL, persistence_worker, NULL) == 0) {
        persistence_thread_running = 1;
    } else {
        LOG_WARN(mux_module, "Could not start the persistent-memory writer; using synchronous saves");
    }
}

void persistent_memory_save(void) {
    if (!current_core.retro_get_memory_data || !current_core.retro_get_memory_size) return;

    pthread_mutex_lock(&persistence_mutex);
    int queued_count = 0;
    for (int index = 0; index < persistent_region_count; index++) {
        if (!regions[index].active) continue;
        if (queue_region(&regions[index], &queued[index]) > 0) queued_count++;
    }

    if (queued_count > 0 && persistence_thread_running)
        pthread_cond_signal(&persistence_wake);
    else if (queued_count > 0)
        write_queued_synchronously();
    pthread_mutex_unlock(&persistence_mutex);
}

int persistent_memory_flush(void) {
    pthread_mutex_lock(&persistence_mutex);
    while (snapshots_pending() || write_in_flight)
        pthread_cond_wait(&persistence_wake, &persistence_mutex);
    const int result = write_failed ? -1 : 0;
    write_failed = 0;
    pthread_mutex_unlock(&persistence_mutex);
    return result;
}

void persistent_memory_shutdown(void) {
    if (persistence_thread_running) {
        pthread_mutex_lock(&persistence_mutex);
        worker_shutdown = 1;
        pthread_cond_broadcast(&persistence_wake);
        while (snapshots_pending() || write_in_flight)
            pthread_cond_wait(&persistence_wake, &persistence_mutex);
        pthread_mutex_unlock(&persistence_mutex);

        pthread_join(persistence_thread, NULL);
        persistence_thread_running = 0;
    }

    for (int index = 0; index < persistent_region_count; index++) {
        free(regions[index].shadow);
        regions[index].shadow = NULL;
        regions[index].shadow_capacity = 0;
        regions[index].shadow_size = 0;

        free(regions[index].preserved);
        regions[index].preserved = NULL;
        regions[index].preserved_capacity = 0;
        regions[index].preserved_size = 0;
        regions[index].active = 0;
        regions[index].path[0] = '\0';

        free(queued[index].data);
        memset(&queued[index], 0, sizeof(queued[index]));
        free(writing[index].data);
        memset(&writing[index], 0, sizeof(writing[index]));
    }
}
