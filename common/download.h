#pragma once

extern int cancel_download;
extern int download_in_progress;
extern volatile int download_finish_result;

extern void (*download_finish_pending_cb)(int);

void download_poll(void);

void set_download_callbacks(void (*callback)(int));

void initiate_download(const char *url, const char *output_path, int show_progress, char *message);
