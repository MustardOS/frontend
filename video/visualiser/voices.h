#pragma once

#define WASABI_VOICE_MAX     8
#define WASABI_VOICE_SAMPLES 256
#define WASABI_VOICE_NAME    24

typedef struct {
    int count;
    int selected;
    char names[WASABI_VOICE_MAX][WASABI_VOICE_NAME];
    float samples[WASABI_VOICE_MAX][WASABI_VOICE_SAMPLES];
} wasabi_voice_snapshot;

void wasabi_voices_open(const char *uri);
void wasabi_voices_close(void);
void wasabi_voices_sync(double position, int active);
int wasabi_voices_snapshot(wasabi_voice_snapshot *snapshot);
int wasabi_voices_count(void);
int wasabi_voices_selected(void);
void wasabi_voices_select(int selected);
int wasabi_voices_mute_mask(void);
