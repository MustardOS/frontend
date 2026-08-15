#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/muxretro.h"
#include "../netplay/netplay.h"
#include "../settings/settings.h"

#define AUDIO_SCRATCH_FRAMES 4096
#define SAMPLE_FIFO_FRAMES   256
#define AUDIO_RING_FRAMES    32768

#define CORE_MIN_LATENCY_CEILING_MS 512

#define AUDIO_PERIOD_SAFE_FRAMES        512
#define AUDIO_PERIOD_SETTLE_MS          3000
#define AUDIO_PERIOD_UNDERRUN_TOLERANCE 8

#define AUDIO_AUTO_RATE_FROM 48000
#define AUDIO_AUTO_RATE_TO   44100

#define AUDIO_FADE_IN_MS            8
#define AUDIO_UNDERRUN_RAMP_MS      1
#define AUDIO_RESUME_PREFILL_MIN_MS 20
#define AUDIO_RESUME_PREFILL_MAX_MS 64

#define DRC_INTEGRAL_DIVISOR 500.0
#define DRC_FILL_SMOOTHING   0.05
#define DRC_OUT_FRAMES       1024

#define HEADROOM_MAX_WAIT_MS 4
#define HEADROOM_SMOOTHING   0.25

#define AUDIO_LATENCY_CEILING_RATIO 2

#define CONTENT_FPS_SMOOTHING     0.02
#define CONTENT_FPS_MIN           20.0
#define CONTENT_FPS_MAX           130.0
#define CONTENT_FPS_SETTLE_FRAMES 180

#define CONTENT_FPS_SNAP_TOLERANCE 0.06
#define CONTENT_FPS_LOCK_FRAMES    150
#define CONTENT_FPS_WINDOW_FRAMES  60
#define CONTENT_FPS_BREAK_RATIO    0.25

#define PACE_QUEUE_GAIN       0.08
#define PACE_CORRECTION_LIMIT 0.20

static SDL_AudioDeviceID audio_dev = 0;

static int opened_freq = 0;
static int opened_channels = 0;
static int opened_period_frames = 0;
static int period_floor_frames = 0;
static uint32_t opened_at_ms = 0;
static int device_paused = 0;
static int resume_pending = 0;
static int audio_muted = 0;

static double core_native_rate = 48000.0;
static SDL_AudioStream *resampler = NULL;

static uint64_t submitted_frames = 0;
static double content_fps_ema = 0.0;
static unsigned content_fps_samples = 0;
static double content_fps_locked = 0.0;
static double content_fps_candidate = 0.0;
static unsigned content_fps_agree = 0;
static double content_fps_window_sum = 0.0;
static unsigned content_fps_window_count = 0;
static double last_batch_ms = 0.0;

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
static uint64_t dropped_frames = 0;

static _Atomic uint32_t fade_in_remaining = 0;
static _Atomic uint32_t fade_in_total = 0;
static uint32_t fade_in_frames = 0;
static uint32_t underrun_ramp_frames = 0;

static double drc_bias = 0.0;
static double drc_fill_avg = -1.0;
static double headroom_queued_avg = -1.0;
static double drc_ratio = 1.0;
static double drc_phase = 0.0;
static int16_t drc_prev_l = 0;
static int16_t drc_prev_r = 0;
static int drc_primed = 0;
static int16_t drc_out_buf[DRC_OUT_FRAMES * 2];

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

static void audio_bridge_maybe_finish_resume(void);

static int16_t scale_sample(const int16_t sample) {
    return (int16_t) ((int32_t) sample * session_settings.volume / 100);
}

typedef struct {
    float prev_in;
    float prev_out;
} audio_filter_state_t;

static audio_filter_state_t filter_state_l = {0};
static audio_filter_state_t filter_state_r = {0};
static float filter_coeff = 0.0f;

static void audio_filter_recompute(void) {
    double cutoff_hz;
    switch (session_settings.audio_filter) {
        case audio_filter_low_pass:
            cutoff_hz = 8000.0;
            break;
        case audio_filter_high_pass:
            cutoff_hz = 400.0;
            break;
        default:
            filter_coeff = 0.0f;
            return;
    }

    const int freq = opened_freq > 0 ? opened_freq : 48000;
    const double dt = 1.0 / (double) freq;
    const double rc = 1.0 / (2.0 * 3.14159265358979323846 * cutoff_hz);

    filter_coeff = (float) (session_settings.audio_filter == audio_filter_high_pass ? rc / (rc + dt) : dt / (rc + dt));
}

static void audio_filter_reset(void) {
    filter_state_l.prev_in = 0.0f;
    filter_state_l.prev_out = 0.0f;
    filter_state_r.prev_in = 0.0f;
    filter_state_r.prev_out = 0.0f;
}

static int16_t apply_audio_filter(const int16_t sample, audio_filter_state_t *st, const int high_pass) {
    const float in = sample;
    float out;

    if (high_pass) {
        out = filter_coeff * (st->prev_out + in - st->prev_in);
    } else {
        out = st->prev_out + filter_coeff * (in - st->prev_out);
    }

    st->prev_in = in;
    st->prev_out = out;

    if (out > 32767.0f) out = 32767.0f;
    if (out < -32768.0f) out = -32768.0f;
    return (int16_t) out;
}

void audio_bridge_apply_filter(void) {
    audio_filter_recompute();
    audio_filter_reset();
}

static void free_resampler(void) {
    if (resampler) {
        SDL_FreeAudioStream(resampler);
        resampler = NULL;
    }
}

static void drc_reset_stream(void) {
    drc_phase = 0.0;
    drc_prev_l = 0;
    drc_prev_r = 0;
    drc_primed = 0;
}

static size_t ring_write_frames(const int16_t *src, const size_t frames) {
    const uint32_t write_idx = ring_write_index;
    const uint32_t read_idx = ring_read_index;
    const uint32_t occupied = write_idx - read_idx;
    const uint32_t free_frames = AUDIO_RING_FRAMES - occupied;

    const size_t to_write = frames > free_frames ? free_frames : frames;
    const uint32_t start = write_idx & (AUDIO_RING_FRAMES - 1);
    const size_t first = to_write < AUDIO_RING_FRAMES - start ? to_write : AUDIO_RING_FRAMES - start;
    const size_t second = to_write - first;

    memcpy(audio_ring + (size_t) start * 2, src, first * 2 * sizeof(int16_t));
    if (second > 0) memcpy(audio_ring, src + first * 2, second * 2 * sizeof(int16_t));
    if (to_write < frames) dropped_frames += frames - to_write;

    ring_write_index = write_idx + (uint32_t) to_write;
    return to_write;
}

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, const int len) {
    (void) userdata;

    const uint32_t requested = (uint32_t) len / (2 * sizeof(int16_t));
    const uint32_t write_idx = ring_write_index;
    const uint32_t read_idx = ring_read_index;
    const uint32_t available = write_idx - read_idx;

    const uint32_t to_read = available < requested ? available : requested;
    int16_t *out = (int16_t *) stream;

    static int16_t last_l = 0;
    static int16_t last_r = 0;

    const uint32_t fade_started = fade_in_remaining;
    const uint32_t fade_total = fade_in_total;
    uint32_t fade_remaining = fade_started;

    if (fade_remaining == 0 || fade_total == 0) {
        const uint32_t start = read_idx & (AUDIO_RING_FRAMES - 1);
        const uint32_t first = to_read < AUDIO_RING_FRAMES - start ? to_read : AUDIO_RING_FRAMES - start;
        const uint32_t second = to_read - first;

        memcpy(out, audio_ring + (size_t) start * 2, (size_t) first * 2 * sizeof(int16_t));
        if (second > 0) memcpy(out + (size_t) first * 2, audio_ring, (size_t) second * 2 * sizeof(int16_t));
    } else {
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
    }

    if (fade_started > 0) fade_in_remaining = fade_remaining;

    if (to_read > 0) {
        last_l = out[(to_read - 1) * 2 + 0];
        last_r = out[(to_read - 1) * 2 + 1];
    }

    if (to_read < requested) {
        const uint32_t missing = requested - to_read;
        const uint32_t ramp = missing < underrun_ramp_frames ? missing : underrun_ramp_frames;

        for (uint32_t i = 0; i < ramp; i++) {
            const int32_t gain = (int32_t) (ramp - i);
            out[(to_read + i) * 2 + 0] = (int16_t) ((int32_t) last_l * gain / (int32_t) ramp);
            out[(to_read + i) * 2 + 1] = (int16_t) ((int32_t) last_r * gain / (int32_t) ramp);
        }

        if (missing > ramp) memset(out + (to_read + ramp) * 2, 0, (size_t) (missing - ramp) * 2 * sizeof(int16_t));

        last_l = 0;
        last_r = 0;

        fade_in_total = fade_in_frames;
        fade_in_remaining = fade_in_frames;
        underrun_count++;
    }

    ring_read_index = read_idx + to_read;
}

static void drc_write_frames(const int16_t *src, const size_t frames) {
    size_t out_count = 0;

    if (drc_ratio < 0.5) drc_ratio = 0.5;

    for (size_t i = 0; i < frames; i++) {
        const int16_t cur_l = src[i * 2 + 0];
        const int16_t cur_r = src[i * 2 + 1];

        if (!drc_primed) {
            drc_prev_l = cur_l;
            drc_prev_r = cur_r;
            drc_primed = 1;
        }

        while (drc_phase < 1.0) {
            drc_out_buf[out_count * 2 + 0] =
                (int16_t) ((double) drc_prev_l + ((double) cur_l - (double) drc_prev_l) * drc_phase);
            drc_out_buf[out_count * 2 + 1] =
                (int16_t) ((double) drc_prev_r + ((double) cur_r - (double) drc_prev_r) * drc_phase);

            drc_phase += drc_ratio;

            if (++out_count == DRC_OUT_FRAMES) {
                ring_write_frames(drc_out_buf, out_count);
                out_count = 0;
            }
        }

        drc_phase -= 1.0;
        drc_prev_l = cur_l;
        drc_prev_r = cur_r;
    }

    if (out_count > 0) ring_write_frames(drc_out_buf, out_count);
}

static void queue_samples(const int16_t *data, const size_t frames) {
    if (!resampler) {
        drc_write_frames(data, frames);
        return;
    }

    const int bytes = (int) (frames * 2 * sizeof(int16_t));
    if (SDL_AudioStreamPut(resampler, data, bytes) != 0) return;

    int avail;
    while ((avail = SDL_AudioStreamAvailable(resampler)) > 0) {
        const int chunk = avail > (int) sizeof(resample_buf) ? (int) sizeof(resample_buf) : avail;
        const int got = SDL_AudioStreamGet(resampler, resample_buf, chunk);
        if (got <= 0) break;
        drc_write_frames((const int16_t *) resample_buf, (size_t) got / (2 * sizeof(int16_t)));
    }
}

static void submit_audio_frames(const int16_t *data, const size_t frames) {
    submitted_frames += frames;

    const int need_volume = session_settings.volume < 100;
    const int need_filter = session_settings.audio_filter != audio_filter_none;

    if (!need_volume && !need_filter) {
        queue_samples(data, frames);
        return;
    }

    const int high_pass = session_settings.audio_filter == audio_filter_high_pass;

    size_t remaining = frames;
    const int16_t *src = data;

    while (remaining > 0) {
        const size_t chunk = remaining > AUDIO_SCRATCH_FRAMES ? AUDIO_SCRATCH_FRAMES : remaining;

        for (size_t i = 0; i < chunk; i++) {
            int16_t l = src[i * 2 + 0];
            int16_t r = src[i * 2 + 1];

            if (need_volume) {
                l = scale_sample(l);
                r = scale_sample(r);
            }

            if (need_filter) {
                l = apply_audio_filter(l, &filter_state_l, high_pass);
                r = apply_audio_filter(r, &filter_state_r, high_pass);
            }

            scratch_buf[i * 2 + 0] = l;
            scratch_buf[i * 2 + 1] = r;
        }

        queue_samples(scratch_buf, chunk);
        src += chunk * 2;
        remaining -= chunk;
    }
}

void audio_bridge_flush_sample_fifo(void) {
    if (sample_fifo_count == 0) return;

    if (audio_dev && !audio_muted) {
        submit_audio_frames(sample_fifo, sample_fifo_count);
        audio_bridge_maybe_finish_resume();

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

    double want_rate = session_settings.sample_rate > 0 ? (double) session_settings.sample_rate : core_sample_rate;
    if (session_settings.sample_rate <= 0 && (int) want_rate == AUDIO_AUTO_RATE_FROM) {
        want_rate = (double) AUDIO_AUTO_RATE_TO;
    }

    want.freq = (int) want_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;

    int want_period = session_settings.audio_period_frames > 0 ? session_settings.audio_period_frames : 512;
    if (period_floor_frames > want_period) want_period = period_floor_frames;

    want.samples = (Uint16) want_period;
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

    fade_in_frames = (uint32_t) opened_freq * AUDIO_FADE_IN_MS / 1000;
    underrun_ramp_frames = (uint32_t) opened_freq * AUDIO_UNDERRUN_RAMP_MS / 1000;
    if (underrun_ramp_frames == 0) underrun_ramp_frames = 1;

    audio_filter_recompute();
    audio_filter_reset();

    ring_write_index = 0;
    ring_read_index = 0;
    underrun_count = 0;
    opened_at_ms = SDL_GetTicks();
    fade_in_remaining = 0;
    fade_in_total = 0;

    drc_bias = 0.0;
    drc_fill_avg = -1.0;
    headroom_queued_avg = -1.0;
    drc_ratio = 1.0;
    drc_reset_stream();

    submitted_frames = 0;
    content_fps_ema = 0.0;
    content_fps_samples = 0;
    content_fps_locked = 0.0;
    content_fps_candidate = 0.0;
    content_fps_agree = 0;
    content_fps_window_sum = 0.0;
    content_fps_window_count = 0;
    last_batch_ms = 0.0;

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

    SDL_PauseAudioDevice(audio_dev, 1);
    device_paused = 1;
    resume_pending = 1;

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
    resume_pending = 0;
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

    // Reopening the device is slow, so only do it when the rate really moved
    if ((int) new_core_rate == (int) core_native_rate) return;

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

    const uint32_t underruns = underrun_count;
    if (underruns > 0) LOG_DEBUG(mux_module, "Audio ring underran %u time(s) this session", underruns);

    if (dropped_frames > 0)
        LOG_WARN(
            mux_module, "Audio ring overran and discarded %llu frame(s) this session (%.2f seconds of sound)",
            (unsigned long long) dropped_frames, opened_freq > 0 ? (double) dropped_frames / (double) opened_freq : 0.0
        );

    LOG_DEBUG(mux_module, "Audio rate control settled at %+.3f%% correction", (1.0 - drc_ratio) * 100.0);

    audio_bridge_discard_sample_fifo();
    close_device();

    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static double snap_content_fps(const double raw) {
    static const double standard[] = {59.94, 50.0, 29.97, 25.0};

    double best = 0.0;
    double best_error = CONTENT_FPS_SNAP_TOLERANCE;

    for (size_t i = 0; i < sizeof(standard) / sizeof(standard[0]); i++) {
        const double diff = raw > standard[i] ? raw - standard[i] : standard[i] - raw;
        const double error = diff / standard[i];
        if (error < best_error) {
            best_error = error;
            best = standard[i];
        }
    }

    return best;
}

void audio_bridge_note_core_frames(const unsigned frames) {
    const uint64_t produced = submitted_frames;
    submitted_frames = 0;

    if (produced && core_native_rate > 0.0) last_batch_ms = (double) produced / core_native_rate * 1000.0;
    if (!frames || !produced || audio_muted || core_native_rate <= 0.0) return;

    const double per_frame = (double) produced / (double) frames;
    if (per_frame < 1.0) return;

    const double implied = core_native_rate / per_frame;
    if (implied < CONTENT_FPS_MIN || implied > CONTENT_FPS_MAX) return;

    content_fps_ema = content_fps_ema <= 0.0
                          ? implied
                          : content_fps_ema * (1.0 - CONTENT_FPS_SMOOTHING) + implied * CONTENT_FPS_SMOOTHING;

    content_fps_window_sum += implied;
    if (++content_fps_window_count >= CONTENT_FPS_WINDOW_FRAMES) {
        const double window = content_fps_window_sum / (double) content_fps_window_count;
        content_fps_window_sum = 0.0;
        content_fps_window_count = 0;

        LOG_DEBUG(
            mux_module, "Content rate window %.2f Hz, average %.2f Hz, locked %.2f Hz", window, content_fps_ema,
            content_fps_locked
        );

        if (content_fps_locked > 0.0) {
            const double drift =
                window > content_fps_locked ? window - content_fps_locked : content_fps_locked - window;
            if (drift / content_fps_locked > CONTENT_FPS_BREAK_RATIO) {
                LOG_INFO(
                    mux_module, "Content rate moved to %.2f Hz, releasing the %.2f Hz lock", window, content_fps_locked
                );
                content_fps_locked = 0.0;
                content_fps_candidate = 0.0;
                content_fps_agree = 0;
                content_fps_ema = window;
            }
        }
    }

    if (content_fps_samples < CONTENT_FPS_SETTLE_FRAMES) {
        content_fps_samples++;
        return;
    }

    const double snapped = snap_content_fps(content_fps_ema);
    if (snapped > 0.0 && snapped == content_fps_candidate) {
        if (content_fps_agree < CONTENT_FPS_LOCK_FRAMES) content_fps_agree++;
        if (content_fps_agree >= CONTENT_FPS_LOCK_FRAMES && content_fps_locked != snapped) {
            content_fps_locked = snapped;
            LOG_INFO(mux_module, "Content rate locked at %.2f Hz", snapped);
        }
        return;
    }

    content_fps_candidate = snapped;
    content_fps_agree = 0;
}

double audio_bridge_locked_content_fps(void) {
    return content_fps_locked;
}

double audio_bridge_pace_target_ms(void) {
    if (last_batch_ms <= 0.0 || !audio_bridge_is_active() || audio_muted) return 0.0;

    const double centre = ((double) audio_bridge_low_water_ms() + (double) audio_bridge_high_water_ms()) * 0.5;
    const double queued = (double) audio_bridge_queued_ms();

    double target = last_batch_ms + (queued - centre) * PACE_QUEUE_GAIN;

    const double lowest = last_batch_ms * (1.0 - PACE_CORRECTION_LIMIT);
    const double highest = last_batch_ms * (1.0 + PACE_CORRECTION_LIMIT);

    if (target < lowest) target = lowest;
    if (target > highest) target = highest;

    return target;
}

double audio_bridge_content_fps(void) {
    return content_fps_samples >= CONTENT_FPS_SETTLE_FRAMES ? content_fps_ema : 0.0;
}

void audio_bridge_get_info(int *freq, int *channels) {
    *freq = opened_freq;
    *channels = opened_channels;
}

int audio_bridge_is_active(void) {
    return audio_dev != 0;
}

static void audio_bridge_trigger_fade_in(void) {
    if (opened_freq == 0) return;

    fade_in_total = fade_in_frames;
    fade_in_remaining = fade_in_frames;
}

void audio_bridge_set_paused(const int pause) {
    if (!audio_dev) return;

    if (pause) {
        if (device_paused && !resume_pending) return;

        resume_pending = 0;
        device_paused = 1;
        SDL_PauseAudioDevice(audio_dev, 1);
        audio_bridge_clear_queued();

        return;
    }

    if (!device_paused && !resume_pending) return;
    if (resume_pending) return;

    resume_pending = 1;
    audio_bridge_maybe_finish_resume();
}

void audio_bridge_set_muted(const int mute) {
    if (mute && !audio_muted) audio_bridge_discard_sample_fifo();
    audio_muted = mute;
}

uint64_t audio_bridge_dropped_frames(void) {
    return dropped_frames;
}

int audio_bridge_is_muted(void) {
    return audio_muted;
}

void audio_bridge_clear_queued(void) {
    audio_bridge_discard_sample_fifo();

    if (audio_dev) {
        SDL_LockAudioDevice(audio_dev);
        const uint32_t read_idx = ring_read_index;
        ring_write_index = read_idx;
        fade_in_remaining = 0;
        fade_in_total = 0;
        SDL_UnlockAudioDevice(audio_dev);
    }

    if (resampler) SDL_AudioStreamClear(resampler);
    last_queued_ms_valid = 0;

    drc_bias = 0.0;
    drc_fill_avg = -1.0;
    headroom_queued_avg = -1.0;
    drc_ratio = 1.0;
    drc_reset_stream();
    audio_filter_reset();
}

Uint32 audio_bridge_queued_ms(void) {
    if (!audio_dev || opened_freq == 0) return 0;

    const uint32_t write_idx = ring_write_index;
    const uint32_t read_idx = ring_read_index;
    const uint32_t occupied_frames = write_idx - read_idx;

    return (Uint32) ((uint64_t) occupied_frames * 1000ULL / (uint64_t) opened_freq);
}

static double period_ms(void) {
    if (!audio_dev || opened_freq == 0) return 0.0;
    return (double) opened_period_frames * 1000.0 / (double) opened_freq;
}

static void compute_latency_targets(const uint32_t floor_ms, uint32_t *low_ms, uint32_t *high_ms) {
    const int profile = session_settings.audio_latency_profile;
    const int valid = profile >= 0 && profile < audio_latency_count;
    const double low_periods = latency_profile_periods[valid ? profile : audio_latency_balanced][0];
    const double high_periods = latency_profile_periods[valid ? profile : audio_latency_balanced][1];

    const double p_ms = period_ms();
    uint32_t low = (uint32_t) (p_ms * low_periods);
    uint32_t high = (uint32_t) (p_ms * high_periods);

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

static uint32_t audio_bridge_resume_target_ms(void) {
    uint32_t target = audio_bridge_low_water_ms();

    if (target < AUDIO_RESUME_PREFILL_MIN_MS) target = AUDIO_RESUME_PREFILL_MIN_MS;
    if (target > AUDIO_RESUME_PREFILL_MAX_MS) target = AUDIO_RESUME_PREFILL_MAX_MS;

    return target;
}

static void audio_bridge_maybe_finish_resume(void) {
    if (!audio_dev || !resume_pending) return;

    const uint32_t target_ms = audio_bridge_resume_target_ms();
    const uint32_t queued_ms = audio_bridge_queued_ms();
    if (queued_ms < target_ms) return;

    audio_bridge_trigger_fade_in();

    resume_pending = 0;
    device_paused = 0;
    last_queued_ms_sample = queued_ms;
    last_queued_ms_valid = 1;

    SDL_PauseAudioDevice(audio_dev, 0);
    LOG_DEBUG(mux_module, "Audio resumed with %ums queued (target %ums)", queued_ms, target_ms);
}

static void period_stability_check(void) {
    if (!audio_dev || opened_period_frames <= 0 || opened_period_frames >= AUDIO_PERIOD_SAFE_FRAMES) return;
    if (SDL_GetTicks() - opened_at_ms < AUDIO_PERIOD_SETTLE_MS) return;
    const uint32_t underruns = underrun_count;
    if (underruns <= AUDIO_PERIOD_UNDERRUN_TOLERANCE) return;

    int stepped = opened_period_frames * 2;
    if (stepped > AUDIO_PERIOD_SAFE_FRAMES) stepped = AUDIO_PERIOD_SAFE_FRAMES;

    LOG_WARN(
        mux_module, "Audio period of %d frames underran %u times; using %d frames for the rest of this session",
        opened_period_frames, underruns, stepped
    );

    period_floor_frames = stepped;
    audio_bridge_apply_sample_rate();
}

void audio_bridge_reset_period_floor(void) {
    period_floor_frames = 0;
}

static void enforce_latency_ceiling(const uint32_t high_ms) {
    if (high_ms == 0) return;

    uint32_t keep_ms = high_ms;
    if (keep_ms < AUDIO_RESUME_PREFILL_MIN_MS) keep_ms = AUDIO_RESUME_PREFILL_MIN_MS;

    const uint32_t ceiling_ms = keep_ms * AUDIO_LATENCY_CEILING_RATIO;
    if (audio_bridge_queued_ms() <= ceiling_ms) return;

    const uint32_t keep = (uint32_t) ((uint64_t) keep_ms * (uint64_t) opened_freq / 1000ULL);

    SDL_LockAudioDevice(audio_dev);
    const uint32_t write_idx = ring_write_index;
    const uint32_t occupied = write_idx - ring_read_index;
    if (occupied > keep) {
        ring_read_index = write_idx - keep;
        dropped_frames += occupied - keep;
    }
    SDL_UnlockAudioDevice(audio_dev);

    LOG_DEBUG(mux_module, "Audio queue reached %ums, skipped ahead to %ums", ceiling_ms, high_ms);
}

void audio_bridge_drc_tick(void) {
    period_stability_check();
    if (!audio_dev || audio_muted || device_paused || opened_freq == 0) return;

    const double max_deviation = (double) session_settings.audio_rate_control / 10000.0;
    if (max_deviation <= 0.0) {
        drc_bias = 0.0;
        drc_ratio = 1.0;
        return;
    }

    uint32_t low, high;
    compute_latency_targets(active_min_latency_ms, &low, &high);
    enforce_latency_ceiling(high);

    const uint64_t target_frames = (uint64_t) (low + high) / 2ULL * (uint64_t) opened_freq / 1000ULL;
    if (target_frames == 0) return;

    const uint32_t write_idx = ring_write_index;
    const uint32_t read_idx = ring_read_index;
    const double fill = (double) (write_idx - read_idx) / (double) target_frames;

    drc_fill_avg = drc_fill_avg < 0.0 ? fill : drc_fill_avg * (1.0 - DRC_FILL_SMOOTHING) + fill * DRC_FILL_SMOOTHING;

    double error = 1.0 - drc_fill_avg;
    if (error > 1.0) error = 1.0;
    if (error < -1.0) error = -1.0;

    drc_bias += error * max_deviation / DRC_INTEGRAL_DIVISOR;
    if (drc_bias > max_deviation) drc_bias = max_deviation;
    if (drc_bias < -max_deviation) drc_bias = -max_deviation;

    double correction = max_deviation * error + drc_bias;
    if (correction > max_deviation) correction = max_deviation;
    if (correction < -max_deviation) correction = -max_deviation;

    drc_ratio = 1.0 - correction;
}

void audio_bridge_wait_for_headroom(const uint32_t budget_ms) {
    audio_bridge_maybe_finish_resume();
    if (device_paused) return;
    if (budget_ms == 0) return;

    const uint32_t queued = audio_bridge_queued_ms();

    headroom_queued_avg = headroom_queued_avg < 0.0
                              ? (double) queued
                              : headroom_queued_avg * (1.0 - HEADROOM_SMOOTHING) + (double) queued * HEADROOM_SMOOTHING;

    const double depth = headroom_queued_avg < (double) queued ? headroom_queued_avg : (double) queued;

    const uint32_t high = audio_bridge_high_water_ms();
    if (depth <= (double) high) return;

    uint32_t wait_ms = (uint32_t) (depth - (double) high);
    if (wait_ms > HEADROOM_MAX_WAIT_MS) wait_ms = HEADROOM_MAX_WAIT_MS;
    if (wait_ms > budget_ms) wait_ms = budget_ms;
    if (wait_ms == 0) return;

    SDL_Delay(wait_ms);
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
    if (netplay_is_active()) {
        buffer_status_cb(true, 50, false);
        return;
    }

    const bool active = audio_dev != 0 && !audio_muted && !device_paused;
    unsigned occupancy_pct = 0;
    bool underrun_likely = false;

    if (active) {
        const uint32_t queued = audio_bridge_queued_ms();
        const uint32_t low = audio_bridge_low_water_ms();
        const uint32_t high = audio_bridge_high_water_ms();

        const uint64_t pct = high > 0 ? (uint64_t) queued * 100ULL / (uint64_t) high : 0;
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
    audio_bridge_maybe_finish_resume();
    return frames;
}
