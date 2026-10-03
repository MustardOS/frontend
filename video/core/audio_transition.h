#pragma once

#include <stddef.h>

int audio_transition_start(const char *uri, size_t playlist_index, int rate, int channels, int seconds);
void audio_transition_cancel(void);
int audio_transition_ready(void);
int audio_transition_uri_matches(const char *uri);
size_t audio_transition_playlist_index(void);
double audio_transition_consumed(void);
int audio_transition_handoff(void);
void audio_transition_mark_handoff(void);
void audio_transition_release(void);
void audio_transition_mix(
    float *output, int current_frames, int requested_frames, int eof, double remaining, int crossfade_seconds
);
int audio_transition_fill_handoff(void *stream, int length, int volume);
