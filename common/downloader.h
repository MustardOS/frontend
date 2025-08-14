#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <pthread.h>
#include "../lvgl/lvgl.h"
#include "ui_common.h"

extern bool cancel_download;
extern bool download_in_progress;

void set_download_callbacks(void (*callback)(int));
void initiate_download(const char *url, const char *output_path, bool showProgress, char *message);