#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "init.h"
#include "archive.h"
#include "fileio.h"
#include "util.h"
#include "ui/nav.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz/miniz.h"
#include "log.h"
#include "language.h"

#define MAX_ARCHIVE_ENTRIES     16384U
#define MAX_ARCHIVE_ENTRY_BYTES (512ULL * 1024ULL * 1024ULL)
#define MAX_ARCHIVE_TOTAL_BYTES (2ULL * 1024ULL * 1024ULL * 1024ULL)
#define MAX_ARCHIVE_RATIO       1000ULL

typedef enum { extraction_idle, extraction_running, extraction_completed } extraction_state;

typedef struct {
    pthread_mutex_t mutex;
    extraction_state state;
    int result;
    char *filename;
    void (*callback)(char *result);
} extraction_control;

static extraction_control extraction = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .state = extraction_idle,
};

typedef struct {
    char *filename;
    char *output_path;
    void (*callback)(char *result);
} extraction_args_t;

static void *extraction_thread(void *arg) {
    extraction_args_t *args = arg;

    const int rc = extract_zip_to_dir(args->filename, args->output_path);

    pthread_mutex_lock(&extraction.mutex);
    extraction.result = rc;
    extraction.filename = args->filename;
    extraction.callback = args->callback;
    extraction.state = extraction_completed;
    pthread_mutex_unlock(&extraction.mutex);

    free(args->output_path);
    free(args);
    return NULL;
}

void extraction_poll(void) {
    pthread_mutex_lock(&extraction.mutex);
    if (extraction.state != extraction_completed) {
        pthread_mutex_unlock(&extraction.mutex);
        return;
    }

    const int result = extraction.result;
    void (*cb)(char *) = extraction.callback;
    char *filename = extraction.filename;
    extraction.result = MUX_EXTRACT_ERR;
    extraction.callback = NULL;
    extraction.filename = NULL;
    extraction.state = extraction_idle;
    pthread_mutex_unlock(&extraction.mutex);

    hide_progress_bar();

    if (result == MUX_EXTRACT_OK) lv_img_cache_invalidate_src(NULL);

    if (cb) cb(result == MUX_EXTRACT_OK ? filename : NULL);

    free(filename);
}

int extract_zip_to_dir_with_progress(const char *filename, const char *output, void (*callback)(char *result)) {
    if (!filename || !*filename || !output || !*output) return -1;

    pthread_mutex_lock(&extraction.mutex);
    if (extraction.state != extraction_idle) {
        pthread_mutex_unlock(&extraction.mutex);
        return -1;
    }

    extraction_args_t *args = calloc(1, sizeof(*args));
    if (!args) {
        pthread_mutex_unlock(&extraction.mutex);
        return -1;
    }

    args->filename = strdup(filename);
    args->output_path = strdup(output);
    args->callback = callback;
    if (!args->filename || !args->output_path) {
        free(args->filename);
        free(args->output_path);
        free(args);
        pthread_mutex_unlock(&extraction.mutex);
        return -1;
    }

    extraction.state = extraction_running;

    pthread_t thread;
    if (pthread_create(&thread, NULL, extraction_thread, args) != 0) {
        extraction.state = extraction_idle;
        free(args->filename);
        free(args->output_path);
        free(args);
        pthread_mutex_unlock(&extraction.mutex);
        return -1;
    }

    pthread_detach(thread);
    pthread_mutex_unlock(&extraction.mutex);
    show_progress_bar(lang.generic.extracting_archive);
    return 0;
}

static int archive_path_is_safe(const char *name) {
    if (!name || !*name || name[0] == '/') return 0;

    const char *component = name;
    while (*component) {
        const char *slash = strchr(component, '/');
        const size_t length = slash ? (size_t) (slash - component) : strlen(component);
        if (length == 2 && component[0] == '.' && component[1] == '.') return 0;
        if (!slash) break;
        component = slash + 1;
    }

    return 1;
}

int extract_zip_to_dir(const char *filename, const char *output) {
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);

    if (!mz_zip_reader_init_file(&zip, filename, 0)) {
        LOG_ERROR(mux_module, "Failed to open ZIP archive!");
        return MUX_EXTRACT_ERR;
    }

    create_directories(output, 0);

    char resolved_output[PATH_MAX];
    if (!realpath(output, resolved_output)) {
        LOG_ERROR(mux_module, "Cannot resolve output path: '%s'", output);
        mz_zip_reader_end(&zip);
        return MUX_EXTRACT_ERR;
    }
    size_t resolved_len = strlen(resolved_output);

    const mz_uint zip_file_count = mz_zip_reader_get_num_files(&zip);
    if (zip_file_count > MAX_ARCHIVE_ENTRIES) {
        LOG_ERROR(mux_module, "Blocked ZIP with too many entries: %u", zip_file_count);
        mz_zip_reader_end(&zip);
        return MUX_EXTRACT_BLOCKED;
    }

    uint64_t total_uncompressed = 0;

    for (mz_uint i = 0; i < zip_file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;

        const char *entry_name = file_stat.m_filename;

        if (!archive_path_is_safe(entry_name)) {
            LOG_ERROR(mux_module, "Blocked unsafe path in ZIP: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }

        if (file_stat.m_uncomp_size > MAX_ARCHIVE_ENTRY_BYTES
            || total_uncompressed > MAX_ARCHIVE_TOTAL_BYTES - file_stat.m_uncomp_size
            || (file_stat.m_comp_size > 0 && file_stat.m_uncomp_size / file_stat.m_comp_size > MAX_ARCHIVE_RATIO)) {
            LOG_ERROR(mux_module, "Blocked oversized ZIP entry: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }
        total_uncompressed += file_stat.m_uncomp_size;

        char dest_file[PATH_MAX];
        const int path_length = snprintf(dest_file, sizeof(dest_file), "%s/%s", resolved_output, entry_name);
        if (path_length < 0 || (size_t) path_length >= sizeof(dest_file)) {
            LOG_ERROR(mux_module, "Blocked overlong ZIP path: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }

        if (strncmp(dest_file, resolved_output, resolved_len) != 0
            || (dest_file[resolved_len] != '/' && dest_file[resolved_len] != '\0')) {
            LOG_ERROR(mux_module, "Blocked path escape in ZIP: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }

        if (file_stat.m_is_directory) {
            create_directories(dest_file, 0);
            continue;
        }

        create_directories(dest_file, 1);

        if (!mz_zip_reader_extract_to_file(&zip, file_stat.m_file_index, dest_file, 0)) {
            LOG_ERROR(mux_module, "File '%s' could not be extracted", dest_file);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_ERR;
        }

        progress_bar_value = zip_file_count ? (int) ((i + 1) * 100 / zip_file_count) : 100;
    }

    mz_zip_reader_end(&zip);
    return 0;
}

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path) {
    mz_zip_archive zip = {0};

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        LOG_ERROR(mux_module, "Could not open archive '%s' - Corrupt?", zip_path);
        return 0;
    }

    const int file_index = mz_zip_reader_locate_file(&zip, file_name, NULL, 0);
    if (file_index == -1) {
        LOG_ERROR(mux_module, "File '%s' not found in archive", file_name);
        mz_zip_reader_end(&zip);
        return 0;
    }

    if (!mz_zip_reader_extract_to_file(&zip, file_index, output_path, 0)) {
        LOG_ERROR(mux_module, "File '%s' could not be extracted", file_name);
        mz_zip_reader_end(&zip);
        return 0;
    }

    mz_zip_reader_end(&zip);
    return 1;
}
