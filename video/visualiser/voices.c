#include "voices.h"

#include <SDL2/SDL.h>
#include <gme/gme.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/audio_source.h"

#define VOICE_RATE          16000
#define VOICE_CHUNK         512
#define VOICE_RESYNC        0.5
#define VOICE_MAX_STEP      (VOICE_RATE / 4)
#define VOICE_SILENCE_LIMIT (VOICE_RATE * 2)
#define VOICE_PERIOD_MS     15

static char voice_path[PATH_MAX];
static int voice_track;
static int voice_available;
static int voice_count;
static int voice_selected;
static char voice_names[WASABI_VOICE_MAX][WASABI_VOICE_NAME];

static SDL_Thread *voice_thread;
static SDL_mutex *voice_lock;
static SDL_atomic_t voice_running;
static double voice_target;
static float voice_ring[WASABI_VOICE_MAX][WASABI_VOICE_SAMPLES];
static int voice_ring_head;

static void voice_names_from(Music_Emu *emu) {
    voice_count = gme_voice_count(emu);
    if (voice_count > WASABI_VOICE_MAX) voice_count = WASABI_VOICE_MAX;
    for (int voice = 0; voice < voice_count; voice++) {
        const char *name = gme_voice_name(emu, voice);
        snprintf(voice_names[voice], sizeof(voice_names[voice]), "%s", name && name[0] ? name : "Voice");
    }
}

void wasabi_voices_open(const char *uri) {
    wasabi_voices_close();
    voice_available = 0;
    voice_count = 0;
    voice_selected = 0;
    voice_path[0] = '\0';
    if (!uri) return;

    wasabi_audio_source source;
    audio_source_resolve(uri, &source);
    if (source.backend != content_audio_chiptune) return;

    Music_Emu *emu = NULL;
    if (gme_open_file(source.path, &emu, gme_info_only) || !emu) return;
    voice_names_from(emu);
    gme_delete(emu);
    if (voice_count < 1) return;

    snprintf(voice_path, sizeof(voice_path), "%s", source.path);
    voice_track = source.subsong > 0 ? source.subsong - 1 : 0;
    voice_available = 1;
}

static Music_Emu *voice_emu_open(const int mute_mask) {
    Music_Emu *emu = NULL;
    if (gme_open_file(voice_path, &emu, VOICE_RATE) || !emu) return NULL;
    gme_ignore_silence(emu, 1);
    if (gme_start_track(emu, voice_track)) {
        gme_delete(emu);
        return NULL;
    }
    gme_mute_voices(emu, mute_mask);
    return emu;
}

static long leading_silence(void) {
    Music_Emu *emu = voice_emu_open(0);
    if (!emu) return 0;
    short buffer[VOICE_CHUNK * 2];
    long frames = 0;
    while (frames < VOICE_SILENCE_LIMIT) {
        if (gme_play(emu, VOICE_CHUNK * 2, buffer)) break;
        int loud = 0;
        for (int index = 0; index < VOICE_CHUNK * 2 && !loud; index++)
            loud = buffer[index] > 8 || buffer[index] < -8;
        if (loud) break;
        frames += VOICE_CHUNK;
    }
    gme_delete(emu);
    return frames;
}

static int voice_worker(void *data __attribute__((unused))) {
    Music_Emu *emus[WASABI_VOICE_MAX] = {0};
    const int count = voice_count;
    for (int voice = 0; voice < count; voice++)
        emus[voice] = voice_emu_open(~(1 << voice));

    const long offset = leading_silence();
    long frames = 0;
    short buffer[VOICE_CHUNK * 2];

    while (SDL_AtomicGet(&voice_running)) {
        SDL_LockMutex(voice_lock);
        const double target_seconds = voice_target;
        SDL_UnlockMutex(voice_lock);

        const long target = (long) (target_seconds * VOICE_RATE) + offset;
        if (frames - target > (long) (VOICE_RESYNC * VOICE_RATE)
            || target - frames > (long) (VOICE_RESYNC * VOICE_RATE)) {
            const int msec = (int) ((double) target * 1000.0 / VOICE_RATE);
            for (int voice = 0; voice < count; voice++) {
                if (emus[voice]) gme_seek(emus[voice], msec);
            }
            frames = target;
        }

        long wanted = target - frames;
        if (wanted > VOICE_MAX_STEP) wanted = VOICE_MAX_STEP;
        while (wanted > 0 && SDL_AtomicGet(&voice_running)) {
            const int chunk = wanted > VOICE_CHUNK ? VOICE_CHUNK : (int) wanted;
            for (int voice = 0; voice < count; voice++) {
                if (!emus[voice] || gme_play(emus[voice], chunk * 2, buffer)) memset(buffer, 0, sizeof(buffer));
                const int keep = chunk < WASABI_VOICE_SAMPLES ? chunk : WASABI_VOICE_SAMPLES;
                SDL_LockMutex(voice_lock);
                for (int index = chunk - keep; index < chunk; index++) {
                    const int slot = (voice_ring_head + index - (chunk - keep)) % WASABI_VOICE_SAMPLES;
                    voice_ring[voice][slot] = ((float) buffer[index * 2] + (float) buffer[index * 2 + 1]) / 65536.0f;
                }
                SDL_UnlockMutex(voice_lock);
            }
            const int keep = chunk < WASABI_VOICE_SAMPLES ? chunk : WASABI_VOICE_SAMPLES;
            SDL_LockMutex(voice_lock);
            voice_ring_head = (voice_ring_head + keep) % WASABI_VOICE_SAMPLES;
            SDL_UnlockMutex(voice_lock);
            frames += chunk;
            wanted -= chunk;
        }
        SDL_Delay(VOICE_PERIOD_MS);
    }

    for (int voice = 0; voice < count; voice++) {
        if (emus[voice]) gme_delete(emus[voice]);
    }
    return 0;
}

static void voices_stop(void) {
    if (!voice_thread) return;
    SDL_AtomicSet(&voice_running, 0);
    SDL_WaitThread(voice_thread, NULL);
    voice_thread = NULL;
}

static void voices_start(void) {
    if (voice_thread || !voice_available) return;
    if (!voice_lock) voice_lock = SDL_CreateMutex();
    if (!voice_lock) return;
    memset(voice_ring, 0, sizeof(voice_ring));
    voice_ring_head = 0;
    SDL_AtomicSet(&voice_running, 1);
    voice_thread = SDL_CreateThread(voice_worker, "wasabi_voices", NULL);
    if (!voice_thread) SDL_AtomicSet(&voice_running, 0);
}

void wasabi_voices_close(void) {
    voices_stop();
}

void wasabi_voices_sync(const double position, const int active) {
    if (!active || !voice_available) {
        voices_stop();
        return;
    }
    voices_start();
    if (!voice_lock) return;
    SDL_LockMutex(voice_lock);
    voice_target = position > 0.0 ? position : 0.0;
    SDL_UnlockMutex(voice_lock);
}

int wasabi_voices_snapshot(wasabi_voice_snapshot *snapshot) {
    if (!snapshot) return 0;
    snapshot->count = voice_available ? voice_count : 0;
    snapshot->selected = voice_selected;
    if (!snapshot->count) return 0;
    memcpy(snapshot->names, voice_names, sizeof(snapshot->names));
    if (!voice_lock) {
        memset(snapshot->samples, 0, sizeof(snapshot->samples));
        return snapshot->count;
    }
    SDL_LockMutex(voice_lock);
    for (int voice = 0; voice < snapshot->count; voice++) {
        for (int index = 0; index < WASABI_VOICE_SAMPLES; index++)
            snapshot->samples[voice][index] = voice_ring[voice][(voice_ring_head + index) % WASABI_VOICE_SAMPLES];
    }
    SDL_UnlockMutex(voice_lock);
    return snapshot->count;
}

int wasabi_voices_count(void) {
    return voice_available ? voice_count : 0;
}

int wasabi_voices_selected(void) {
    return voice_selected;
}

void wasabi_voices_select(const int selected) {
    if (!voice_available || voice_count < 1) {
        voice_selected = 0;
        return;
    }
    voice_selected = selected < 0 ? voice_count : selected > voice_count ? 0 : selected;
}

int wasabi_voices_mute_mask(void) {
    if (!voice_available || voice_selected < 1 || voice_selected > voice_count) return 0;
    return ~(1 << (voice_selected - 1)) & ((1 << voice_count) - 1);
}
