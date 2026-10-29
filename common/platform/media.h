#pragma once

#include <stddef.h>

#define MEDIA_BACKGROUND_DIR   "/run/muos/wasabi"
#define MEDIA_BACKGROUND_PID   MEDIA_BACKGROUND_DIR "/background.pid"
#define MEDIA_BACKGROUND_STATE MEDIA_BACKGROUND_DIR "/background.state"
#define MEDIA_BACKGROUND_TRACK MEDIA_BACKGROUND_DIR "/track"

int media_background_active(void);
int media_track_changed(char *text, size_t size);
