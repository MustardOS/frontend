#pragma once

#include <stddef.h>
#include <stdatomic.h>

extern _Atomic int cancel_download;
extern _Atomic int download_in_progress;

enum download_result {
    download_result_ok = 0,
    download_result_invalid = -1,
    download_result_memory = -2,
    download_result_transport = -3,
    download_result_http = -4,
    download_result_size = -5,
    download_result_publish = -6,
    download_result_thread = -7,
    download_result_storage_missing = -8,
    download_result_storage_read_only = -9,
    download_result_storage_full = -10,
    download_result_storage_error = -11
};

void download_poll(void);

void set_download_callbacks(void (*callback)(int));

void set_download_progress_span(int index, int total);

int initiate_download(const char *url, const char *output_path, int show_progress, char *message);

int initiate_download_limited(
    const char *url, const char *output_path, size_t max_bytes, int show_progress, char *message
);

const char *download_result_message(int result);

const char *download_storage_message(int result);
