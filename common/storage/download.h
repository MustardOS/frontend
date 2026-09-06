#pragma once

#include <stdatomic.h>

extern _Atomic int cancel_download;
extern _Atomic int download_in_progress;

void download_poll(void);

void set_download_callbacks(void (*callback)(int));

int initiate_download(const char *url, const char *output_path, int show_progress, char *message);
