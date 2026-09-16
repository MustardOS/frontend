#pragma once

#include <stddef.h>
#include <stdatomic.h>

extern _Atomic int cancel_download;
extern _Atomic int download_in_progress;

void download_poll(void);

void set_download_callbacks(void (*callback)(int));

void set_download_progress_span(int index, int total);

int initiate_download(const char *url, const char *output_path, int show_progress, char *message);

int initiate_download_limited(
    const char *url, const char *output_path, size_t max_bytes, int show_progress, char *message
);
