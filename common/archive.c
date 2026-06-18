#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "init.h"
#include "archive.h"
#include "fileio.h"
#include "util.h"
#include "ui/nav.h"
#include "miniz/miniz.h"
#include "log.h"
#include "language.h"
#include "skip.h"

static void (*extraction_finish_cb)(char *result) = NULL;

static void (*extraction_finish_pending_cb)(char *result) = NULL;

static volatile int extraction_finish_result = INT_MIN;
static char *extraction_pending_filename = NULL;

typedef struct {
    char *filename;
    char *output_path;
} extraction_args_t;

static void *extraction_thread(void *arg) {
    extraction_args_t *args = (extraction_args_t *) arg;

    int rc = extract_zip_to_dir(args->filename, args->output_path);

    extraction_finish_pending_cb = extraction_finish_cb;
    extraction_finish_cb = NULL;
    extraction_pending_filename = args->filename;
    args->filename = NULL;
    extraction_finish_result = rc;

    free(args->output_path);
    free(args);
    return NULL;
}

void extraction_poll(void) {
    if (extraction_finish_result == INT_MIN) return;

    int result = extraction_finish_result;
    extraction_finish_result = INT_MIN;

    void (*cb)(char *) = extraction_finish_pending_cb;
    extraction_finish_pending_cb = NULL;

    char *filename = extraction_pending_filename;
    extraction_pending_filename = NULL;

    hide_progress_bar();

    if (cb) cb(result == MUX_EXTRACT_OK ? filename : NULL);

    free(filename);
}

void extract_zip_to_dir_with_progress(const char *filename, const char *output, void (*callback)(char *result)) {
    extraction_finish_cb = callback;
    show_progress_bar(lang.GENERIC.EXTRACTING_ARCHIVE);

    extraction_args_t *args = mux_malloc(sizeof(*args));

    args->filename = mux_strdup(filename);
    args->output_path = mux_strdup(output);

    pthread_t tid;
    pthread_create(&tid, NULL, extraction_thread, args);
    pthread_detach(tid);
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

    mz_uint zip_file_count = mz_zip_reader_get_num_files(&zip);

    for (mz_uint i = 0; i < zip_file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;

        const char *entry_name = file_stat.m_filename;

        if (entry_name[0] == '/' || strstr(entry_name, "..")) {
            LOG_ERROR(mux_module, "Blocked unsafe path in ZIP: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }

        char dest_file[PATH_MAX];
        snprintf(dest_file, sizeof(dest_file), "%s/%s", resolved_output, entry_name);

        if (strncmp(dest_file, resolved_output, resolved_len) != 0 ||
            (dest_file[resolved_len] != '/' && dest_file[resolved_len] != '\0')) {
            LOG_ERROR(mux_module, "Blocked path escape in ZIP: '%s'", entry_name);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_BLOCKED;
        }

        if (file_stat.m_is_directory) {
            create_directories(dest_file, 0);
            continue;
        }

        if (file_exist(dest_file)) {
            remove(dest_file);
        }

        if (!mz_zip_reader_extract_to_file(&zip, file_stat.m_file_index, dest_file, 0)) {
            LOG_ERROR(mux_module, "File '%s' could not be extracted", dest_file);
            mz_zip_reader_end(&zip);
            return MUX_EXTRACT_ERR;
        } else {
            if (ends_with(dest_file, ".png")) {
                char img_source[MAX_BUFFER_SIZE];
                snprintf(img_source, sizeof(img_source), "M:%s", dest_file);
                lv_img_cache_invalidate_src(img_source);
            }
        }

        progress_bar_value = (int) (((i + 1) * 100) / zip_file_count);
    }

    mz_zip_reader_end(&zip);
    return 0;
}

int extract_file_from_zip(const char *zip_path, const char *filename, const char *output) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path, 0)) {
        LOG_ERROR(mux_module, "Could not open archive '%s' - Corrupt?", zip_path);
        return 0;
    }

    int file_index = mz_zip_reader_locate_file(&zip, filename, NULL, 0);
    if (file_index == -1) {
        LOG_ERROR(mux_module, "File '%s' not found in archive", filename);
        mz_zip_reader_end(&zip);
        return 0;
    }

    if (!mz_zip_reader_extract_to_file(&zip, file_index, output, 0)) {
        LOG_ERROR(mux_module, "File '%s' could not be extracted", filename);
        mz_zip_reader_end(&zip);
        return 0;
    }

    mz_zip_reader_end(&zip);
    return 1;
}
