#include <stdatomic.h>
#include <SDL2/SDL.h>
#include <string.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"

#define AUDIO_SCRATCH_FRAMES 4096
#define SAMPLE_FIFO_FRAMES   256
#define AUDIO_RING_FRAMES    32768

#define CORE_MIN_LATENCY_CEILING_MS 512

#define AUDIO_FADE_IN_MS 8

static SDL_AudioDeviceID audio_dev = 0;

static int opened_freq = 0;
static int opened_channels = 0;
static int opened_period_frames = 0;
static int device_paused = 0;
static int audio_muted = 0;

static double core_native_rate = 48000.0;
static SDL_AudioStream *resampler = NULL;

static int16_t scratch_buf[AUDIO_SCRATCH_FRAMES * 2];
static uint8_t resample_buf[AUDIO_SCRATCH_FRAMES * 2 * sizeof(int16_t)];

static int16_t sample_fifo[SAMPLE_FIFO_FRAMES * 2];
static size_t sample_fifo_count = 0;

static uint64_t single_sample_calls = 0;
static uint64_t single_sample_flushes = 0;
static size_t single_sample_max_batch = 0;

static int16_t audio_ring[AUDIO_RING_FRAMES * 2];
static _Atomic uint32_t ring_write_index = 0;
static _Atomic uint32_t ring_read_index = 0;
static _Atomic uint32_t underrun_count = 0;

static _Atomic uint32_t fade_in_remaining = 0;

static const double latency_profile_periods[audio_latency_count][2] = {
    [audio_latency_low] = {2.0, 3.0},
    [audio_latency_balanced] = {3.0, 5.0},
    [audio_latency_compat] = {6.0, 8.0},
};

static uint32_t pending_min_latency_ms = 0;
static int min_latency_pending = 0;
static uint32_t active_min_latency_ms = 0;

static retro_audio_buffer_status_callback_t buffer_status_cb = NULL;
static uint32_t last_queued_ms_sample = 0;
static int last_queued_ms_valid = 0;

static int16_t scale_sample(const int16_t sample) {
    return (int16_t) ((int32_t) sample * session_settings.volume / 100);
}

static void free_resampler(void) {
    if (resampler) {
        SDL_FreeAudioStream(resampler);
        resampler = NULL;
    }
}

static size_t ring_write_frames(const int16_t *src, const size_t frames) {
    const uint32_t write_idx = atomic_load_explicit(&ring_write_index, memory_order_relaxed);
    const uint32_t read_idx = atomic_load_explicit(&ring_read_index, memory_order_acquire);
    const uint32_t occupied = write_idx - read_idx;
    const uint32_t free_frames = AUDIO_RING_FRAMES - occupied;

    const size_t to_write = frames > free_frames ? free_frames : frames;

    for (size_t i = 0; i < to_write; i++) {
        const uint32_t pos = (write_idx + (uint32_t) i) & (AUDIO_RING_FRAMES - 1);
        audio_ring[pos * 2 + 0] = src[i * 2 + 0];
        audio_ring[pos * 2 + 1] = src[i * 2 + 1];
    }

    atomic_store_explicit(&ring_write_index, write_idx + (uint32_t) to_write, memory_order_release);
    return to_write;
}

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, const int len) {
    (void) userdata;

    const uint32_t requested = (uint32_t) len / (2 * sizeof(int16_t));
    const uint32_t write_idx = atomic_load_explicit(&ring_write_index, memory_order_acquire);
    const uint32_t read_idx = atomic_load_explicit(&ring_read_index, memory_order_relaxed);
    const uint32_t available = write_idx - read_idx;

    const uint32_t to_read = available < requested ? available : requested;
    int16_t *out = (int16_t *) stream;

    uint32_t fade_remaining = atomic_load_explicit(&fade_in_remaining, memory_order_relaxed);
    const uint32_t fade_total = fade_remaining;

    for (uint32_t i = 0; i < to_read; i++) {
        const uint32_t pos = (read_idx + i) & (AUDIO_RING_FRAMES - 1);
        int16_t l = audio_ring[pos * 2 + 0];
        int16_t r = audio_ring[pos * 2 + 1];

        if (fade_remaining > 0) {
            const int32_t step = (int32_t) (fade_total - fade_remaining + 1);
            l = (int16_t) ((int32_t) l * step / (int32_t) fade_total);
            r = (int16_t) ((int32_t) r * step / (int32_t) fade_total);
            fade_remaining--;
        }

        out[i * 2 + 0] = l;
        out[i * 2 + 1] = r;
    }

    if (fade_total > 0) atomic_store_explicit(&fade_in_remaining, fade_remaining, memory_order_relaxed);

    if (to_read < requested) {
        memset(out + to_read * 2, 0, (size_t) (requested - to_read) * 2 * sizeof(int16_t));
        atomic_fetch_add_explicit(&underrun_count, 1, memory_order_relaxed);
    }

    atomic_store_explicit(&ring_read_index, read_idx + to_read, memory_order_release);
}

static void queue_samples(const int16_t *data, const size_t frames) {
    if (!resampler) {
        ring_write_frames(data, frames);
        return;
    }

    const int bytes = (int) (frames * 2 * sizeof(int16_t));
    if (SDL_AudioStreamPut(resampler, data, bytes) != 0) return;

    int avail;
    while ((avail = SDL_AudioStreamAvailable(resampler)) > 0) {
        const int chunk = avail > (int) sizeof(resample_buf) ? (int) sizeof(resample_buf) : avail;
        const int got = SDL_AudioStreamGet(resampler, resample_buf, chunk);
        if (got <= 0) break;
        ring_write_frames((const int16_t *) resample_buf, (size_t) got / (2 * sizeof(int16_t)));
    }
}

static void submit_audio_frames(const int16_t *data, const size_t frames) {
    if (session_settings.volume >= 100) {
        queue_samples(data, frames);
        return;
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
}

void audio_bridge_flush_sample_fifo(void) {
    if (sample_fifo_count == 0) return;

    if (audio_dev && !audio_muted) {
        submit_audio_frames(sample_fifo, sample_fifo_count);

        single_sample_flushes++;
        if (sample_fifo_count > single_sample_max_batch) single_sample_max_batch = sample_fifo_count;
    }

    sample_fifo_count = 0;
}

void audio_bridge_discard_sample_fifo(void) {
    sample_fifo_count = 0;
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
    want.samples = (Uint16) (session_settings.audio_period_frames > 0 ? session_settings.audio_period_frames : 512);
    want.callback = audio_callback;
    want.userdata = NULL;

    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (audio_dev == 0) {
        LOG_ERROR(mux_module, "Failed to open audio device: %s", SDL_GetError());
        return -1;
    }

    opened_freq = have.freq;
    opened_channels = have.channels;
    opened_period_frames = (int) have.samples;

    atomic_store_explicit(&ring_write_index, 0, memory_order_relaxed);
    atomic_store_explicit(&ring_read_index, 0, memory_order_relaxed);
    atomic_store_explicit(&underrun_count, 0, memory_order_relaxed);

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
    LOG_SUCCESS(
        mux_module, "Audio device opened at %d Hz (core native %d Hz), period %d frames", have.freq,
        (int) core_sample_rate, opened_period_frames
    );
    return 0;
}

static void close_device(void) {
    if (audio_dev) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }

    free_resampler();

    opened_freq = 0;
    opened_channels = 0;
    opened_period_frames = 0;
    device_paused = 0;
    last_queued_ms_valid = 0;
}

void audio_bridge_apply_sample_rate(void) {
    if (!audio_dev) return;
    audio_bridge_discard_sample_fifo();
    close_device();
    audio_bridge_open(core_native_rate);
}

void audio_bridge_reconfigure_rate(const double new_core_rate) {
    if (new_core_rate <= 0.0 || !audio_dev) return;
    core_native_rate = new_core_rate;
    audio_bridge_apply_sample_rate();
}

void audio_bridge_close(void) {
    if (single_sample_calls > 0) {
        LOG_DEBUG(
            mux_module, "Core used single-sample audio: %llu calls, %llu flushes, avg batch %.1f, max batch %zu",
            (unsigned long long) single_sample_calls, (unsigned long long) single_sample_flushes,
            single_sample_flushes ? (double) single_sample_calls / (double) single_sample_flushes : 0.0,
            single_sample_max_batch
        );
    }

    const uint32_t underruns = atomic_load_explicit(&underrun_count, memory_order_relaxed);
    if (underruns > 0) LOG_DEBUG(mux_module, "Audio ring underran %u time(s) this session", underruns);

    audio_bridge_discard_sample_fifo();
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

static void audio_bridge_trigger_fade_in(void) {
    if (!audio_dev || opened_freq == 0) return;
    const uint32_t frames = (uint32_t) opened_freq * AUDIO_FADE_IN_MS / 1000;
    atomic_store_explicit(&fade_in_remaining, frames, memory_order_relaxed);
}

void audio_bridge_set_paused(const int pause) {
    if (!audio_dev || pause == device_paused) return;

    if (pause) {
        audio_bridge_clear_queued();
    } else {
        audio_bridge_trigger_fade_in();
    }

    device_paused = pause;
    SDL_PauseAudioDevice(audio_dev, pause);
}

void audio_bridge_set_muted(const int mute) {
    if (mute && !audio_muted) audio_bridge_discard_sample_fifo();
    audio_muted = mute;
}

int audio_bridge_is_muted(void) {
    return audio_muted;
}

void audio_bridge_clear_queued(void) {
    audio_bridge_discard_sample_fifo();

    if (audio_dev) {
        SDL_LockAudioDevice(audio_dev);
        const uint32_t read_idx = atomic_load_explicit(&ring_read_index, memory_order_relaxed);
        atomic_store_explicit(&ring_write_index, read_idx, memory_order_relaxed);
        SDL_UnlockAudioDevice(audio_dev);
    }

    if (resampler) SDL_AudioStreamClear(resampler);
    last_queued_ms_valid = 0;
}

Uint32 audio_bridge_queued_ms(void) {
    if (!audio_dev || opened_freq == 0) return 0;

    const uint32_t write_idx = atomic_load_explicit(&ring_write_index, memory_order_acquire);
    const uint32_t read_idx = atomic_load_explicit(&ring_read_index, memory_order_acquire);
    const uint32_t occupied_frames = write_idx - read_idx;

    return (Uint32) (((uint64_t) occupied_frames * 1000ULL) / (uint64_t) opened_freq);
}

static uint32_t period_ms(void) {
    if (!audio_dev || opened_freq == 0) return 0;
    return (uint32_t) (((uint64_t) opened_period_frames * 1000ULL) / (uint64_t) opened_freq);
}

static void compute_latency_targets(const uint32_t floor_ms, uint32_t *low_ms, uint32_t *high_ms) {
    const int profile = session_settings.audio_latency_profile;
    const int valid = profile >= 0 && profile < audio_latency_count;
    const double low_periods = latency_profile_periods[valid ? profile : audio_latency_balanced][0];
    const double high_periods = latency_profile_periods[valid ? profile : audio_latency_balanced][1];

    const uint32_t p_ms = period_ms();
    uint32_t low = (uint32_t) ((double) p_ms * low_periods);
    uint32_t high = (uint32_t) ((double) p_ms * high_periods);

    if (floor_ms > low) {
        const uint32_t spread = high - low;
        low = floor_ms;
        high = low + spread;
    }

    *low_ms = low;
    *high_ms = high;
}

uint32_t audio_bridge_low_water_ms(void) {
    uint32_t low, high;
    compute_latency_targets(active_min_latency_ms, &low, &high);
    return low;
}

uint32_t audio_bridge_high_water_ms(void) {
    uint32_t low, high;
    compute_latency_targets(active_min_latency_ms, &low, &high);
    return high;
}

void audio_bridge_wait_for_headroom(void) {
    const uint32_t queued = audio_bridge_queued_ms();
    const uint32_t high = audio_bridge_high_water_ms();
    if (queued > high) SDL_Delay(queued - high);
}

void audio_bridge_request_min_latency(const uint32_t ms) {
    pending_min_latency_ms = ms > CORE_MIN_LATENCY_CEILING_MS ? CORE_MIN_LATENCY_CEILING_MS : ms;
    min_latency_pending = 1;
}

void audio_bridge_apply_pending_min_latency(void) {
    if (!min_latency_pending) return;
    min_latency_pending = 0;

    const uint32_t requested = pending_min_latency_ms;
    if (requested == active_min_latency_ms) return;

    uint32_t profile_low, profile_high;
    compute_latency_targets(0, &profile_low, &profile_high);

    const uint32_t old_active = active_min_latency_ms;
    active_min_latency_ms = requested;

    if (requested > profile_low && requested > old_active) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Core requested %ums audio latency", requested);
        pause_menu_show_toast(msg);
        LOG_INFO(
            mux_module, "Core raised effective audio latency to %ums (profile default %ums)", requested, profile_low
        );
    }
}

void audio_bridge_set_buffer_status_callback(const retro_audio_buffer_status_callback_t cb) {
    buffer_status_cb = cb;
    last_queued_ms_valid = 0;
}

void audio_bridge_notify_buffer_status(void) {
    if (!buffer_status_cb) return;

    const bool active = audio_dev != 0 && !audio_muted;
    unsigned occupancy_pct = 0;
    bool underrun_likely = false;

    if (active) {
        const uint32_t queued = audio_bridge_queued_ms();
        const uint32_t low = audio_bridge_low_water_ms();
        const uint32_t high = audio_bridge_high_water_ms();

        const uint64_t pct = high > 0 ? ((uint64_t) queued * 100ULL) / (uint64_t) high : 0;
        occupancy_pct = (unsigned) (pct > 100 ? 100 : pct);

        if (last_queued_ms_valid) {
            const int32_t trend = (int32_t) queued - (int32_t) last_queued_ms_sample;
            underrun_likely = queued <= low && trend <= 0;
        } else {
            underrun_likely = queued <= low;
        }

        last_queued_ms_sample = queued;
        last_queued_ms_valid = 1;
    }

    buffer_status_cb(active, occupancy_pct, underrun_likely);
}

void mux_retro_audio_sample_cb(const int16_t left, const int16_t right) {
    if (!audio_dev || audio_muted) return;

    single_sample_calls++;

    sample_fifo[sample_fifo_count * 2 + 0] = left;
    sample_fifo[sample_fifo_count * 2 + 1] = right;

    if (++sample_fifo_count == SAMPLE_FIFO_FRAMES) audio_bridge_flush_sample_fifo();
}

size_t mux_retro_audio_sample_batch_cb(const int16_t *data, const size_t frames) {
    if (!audio_dev || !data || audio_muted) return frames;

    submit_audio_frames(data, frames);
    return frames;
}
