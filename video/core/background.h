#pragma once

#include <stddef.h>

#include <common/platform/media.h>

#define WASABI_BACKGROUND_PID   MEDIA_BACKGROUND_PID
#define WASABI_BACKGROUND_STATE MEDIA_BACKGROUND_STATE
#define WASABI_BACKGROUND_TRACK MEDIA_BACKGROUND_TRACK

int wasabi_background_launch(const char *source, size_t index, const char *track, double position);
int wasabi_background_run(const char *source, size_t index, const char *track, double position);
int wasabi_background_takeover(char *track, size_t track_size, double *position, char *source, size_t source_size);
