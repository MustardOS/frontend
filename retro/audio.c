#include <SDL2/SDL.h>
#include <string.h>
#include "../common/init.h"
#include "../common/log.h"
#include "muxretro.h"
#include "settings.h"

#define AUDIO_SCRATCH_FRAMES 4096

static SDL_AudioDeviceID audio_dev = 0;
static Uint32 bytes_per_ms = 0;

static int opened_freq = 0;
static int opened_channels = 0;
static int device_paused = 0;
static int audio_muted = 0;

static double core_native_rate = 48000.0;
static SDL_AudioStream *resampler = NULL;

static int16_t scratch_buf[AUDIO_SCRATCH_FRAMES * 2];
static uint8_t resample_buf[AUDIO_SCRATCH_FRAMES * 2 * sizeof(int16_t)];

static int16_t scale_sample(const int16_t sample) {
    return (int16_t) ((int32_t) sample * session_settings.volume / 100);
}

static void free_resampler(void) {
    if (resampler) {
        SDL_FreeAudioStream(resampler);
        resampler = NULL;
    }
}

static void queue_samples(const int16_t *data, const size_t frames) {
    const int bytes = (int) (frames * 2 * sizeof(int16_t));

    if (!resampler) {
        SDL_QueueAudio(audio_dev, data, (Uint32) bytes);
        return;
    }

    if (SDL_AudioStreamPut(resampler, data, bytes) != 0) return;

    int avail;
    while ((avail = SDL_AudioStreamAvailable(resampler)) > 0) {
        const int chunk = avail > (int) sizeof(resample_buf) ? (int) sizeof(resample_buf) : avail;
        const int got = SDL_AudioStreamGet(resampler, resample_buf, chunk);
        if (got <= 0) break;
        SDL_QueueAudio(audio_dev, resample_buf, (Uint32) got);
    }
}

int audio_bridge_open(const double core_sample_rate) {
    core_native_rate = core_sample_rate;

    SDL_AudioSpec want;
    SDL_AudioSpec have;
    memset(&want, 0, sizeof(want));

    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        LOG_ERROR(mux_module, "Failed to init SDL audio subsystem: %s", SDL_GetError());
        return -1;
    }

    const double want_rate =
        session_settings.sample_rate > 0 ? (double) session_settings.sample_rate : core_sample_rate;

    want.freq = (int) want_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = NULL;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_dev == 0) {
        LOG_ERROR(mux_module, "Failed to open audio device: %s", SDL_GetError());
        return -1;
    }

    bytes_per_ms = (Uint32) (have.freq * have.channels * SDL_AUDIO_BITSIZE(have.format) / 8 / 1000);
    opened_freq = have.freq;
    opened_channels = have.channels;

    free_resampler();
    if ((int) core_sample_rate != have.freq) {
        resampler = SDL_NewAudioStream(AUDIO_S16SYS, 2, (int) core_sample_rate, AUDIO_S16SYS, 2, have.freq);
        if (!resampler) {
            LOG_ERROR(
                mux_module, "Failed to create audio resampler (%d -> %d Hz): %s", (int) core_sample_rate, have.freq,
                SDL_GetError()
            );
        }
    }

    SDL_PauseAudioDevice(audio_dev, 0);
    device_paused = 0;
    LOG_SUCCESS(mux_module, "Audio device opened at %d Hz (core native %d Hz)", have.freq, (int) core_sample_rate);
    return 0;
}

static void close_device(void) {
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }

    free_resampler();

    bytes_per_ms = 0;
    opened_freq = 0;
    opened_channels = 0;
    device_paused = 0;
}

void audio_bridge_apply_sample_rate(void) {
    if (!audio_dev) return;
    close_device();
    audio_bridge_open(core_native_rate);
}

void audio_bridge_close(void) {
    close_device();

    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void audio_bridge_get_info(int *freq, int *channels) {
    *freq = opened_freq;
    *channels = opened_channels;
}

int audio_bridge_is_active(void) {
    return audio_dev != 0;
}

void audio_bridge_set_paused(const int pause) {
    if (!audio_dev || pause == device_paused) return;
    device_paused = pause;
    SDL_PauseAudioDevice(audio_dev, pause);
}

void audio_bridge_set_muted(const int mute) {
    audio_muted = mute;
}

int audio_bridge_is_muted(void) {
    return audio_muted;
}

void audio_bridge_clear_queued(void) {
    if (audio_dev) SDL_ClearQueuedAudio(audio_dev);
    if (resampler) SDL_AudioStreamClear(resampler);
}

Uint32 audio_bridge_queued_ms(void) {
    if (!audio_dev || bytes_per_ms == 0) return 0;
    return SDL_GetQueuedAudioSize(audio_dev) / bytes_per_ms;
}

void mux_retro_audio_sample_cb(const int16_t left, const int16_t right) {
    if (!audio_dev || audio_muted) return;

    const int16_t frame[2] = {scale_sample(left), scale_sample(right)};
    queue_samples(frame, 1);
}

size_t mux_retro_audio_sample_batch_cb(const int16_t *data, const size_t frames) {
    if (!audio_dev || !data || audio_muted) return frames;

    if (session_settings.volume >= 100) {
        queue_samples(data, frames);
        return frames;
    }

    size_t remaining = frames;
    const int16_t *src = data;

    while (remaining > 0) {
        const size_t chunk = remaining > AUDIO_SCRATCH_FRAMES ? AUDIO_SCRATCH_FRAMES : remaining;

        for (size_t i = 0; i < chunk * 2; i++)
            scratch_buf[i] = scale_sample(src[i]);

        queue_samples(scratch_buf, chunk);
        src += chunk * 2;
        remaining -= chunk;
    }

    return frames;
}
