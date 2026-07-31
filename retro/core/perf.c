#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/display.h"
#include "perf.h"
#include "muxretro.h"

#define PERF_HISTORY 1024

typedef struct {
    float samples[PERF_HISTORY];
    unsigned next;
    unsigned count;
    double sum;
} perf_series;

static perf_series series[perf_stage_count];
static uint64_t frame_start;
static uint64_t input_change_start;
static uint64_t last_present;
static unsigned batch_iterations;
static unsigned batch_frames;
static unsigned batch_frames_peak;
static unsigned batch_catchup;
static double pending_draw_ms;
static double pending_flip_ms;
static int pending_present_timing;
static double ticks_to_ms;
static int hud_active;
static int capture_active;
static int enabled;

static double elapsed_ms(const uint64_t start) {
    return (double) (SDL_GetPerformanceCounter() - start) * ticks_to_ms;
}

static void push(perf_series *s, const double ms) {
    if (s->count == PERF_HISTORY) {
        s->sum -= s->samples[s->next];
    } else {
        s->count++;
    }

    s->samples[s->next] = (float) ms;
    s->sum += ms;
    s->next = (s->next + 1) % PERF_HISTORY;
}

static double mean(const perf_series *s) {
    return s->count ? s->sum / (double) s->count : 0.0;
}

static double peak(const perf_series *s) {
    double highest = 0.0;
    for (unsigned i = 0; i < s->count; i++)
        if (s->samples[i] > highest) highest = s->samples[i];

    return highest;
}

static int compare_sample(const void *a, const void *b) {
    const float x = *(const float *) a;
    const float y = *(const float *) b;
    return x < y ? -1 : x > y;
}

static double percentile95(const perf_series *s) {
    if (!s->count) return 0.0;

    static float sorted[PERF_HISTORY];
    memcpy(sorted, s->samples, sizeof(float) * s->count);
    qsort(sorted, s->count, sizeof(float), compare_sample);

    return sorted[(s->count - 1) * 95 / 100];
}

static void reset(void) {
    memset(series, 0, sizeof(series));
    frame_start = SDL_GetPerformanceCounter();
    input_change_start = 0;
    last_present = 0;
    batch_iterations = 0;
    batch_frames = 0;
    batch_frames_peak = 0;
    batch_catchup = 0;
    pending_present_timing = 0;
}

static void note_present_timing(const double draw_ms, const double flip_ms) {
    pending_draw_ms = draw_ms;
    pending_flip_ms = flip_ms;
    pending_present_timing = 1;
}

void perf_init(void) {
    display_set_present_timing(NULL);
    ticks_to_ms = 1000.0 / (double) SDL_GetPerformanceFrequency();
    hud_active = 0;
    capture_active = 0;
    enabled = 0;
    reset();
}

static void sync_enabled(void) {
    const int wanted = hud_active || capture_active;
    if (wanted == enabled) return;

    enabled = wanted;
    display_set_present_timing(enabled ? note_present_timing : NULL);
    if (enabled) reset();
}

void perf_set_hud_active(const int active) {
    hud_active = !!active;
    sync_enabled();
}

void perf_set_capture_active(const int active) {
    capture_active = !!active;
    sync_enabled();
    if (capture_active) reset();
}

int perf_is_capture_active(void) {
    return capture_active;
}

int perf_is_enabled(void) {
    return enabled;
}

int perf_has_samples(void) {
    return series[perf_stage_frame].count != 0;
}

uint64_t perf_begin(void) {
    return enabled ? SDL_GetPerformanceCounter() : 0;
}

void perf_end(const enum perf_stage stage, const uint64_t start) {
    if (!enabled || !start || (unsigned) stage >= perf_stage_count) return;
    push(&series[stage], elapsed_ms(start));

    if (stage == perf_stage_present && pending_present_timing) {
        pending_present_timing = 0;
        push(&series[perf_stage_present_draw], pending_draw_ms);
        push(&series[perf_stage_present_flip], pending_flip_ms);
    }
}

void perf_record(const enum perf_stage stage, const double ms) {
    if (!enabled || (unsigned) stage >= perf_stage_count) return;
    push(&series[stage], ms);
}

void perf_frame_complete(const int record) {
    if (!enabled) return;

    const uint64_t now = SDL_GetPerformanceCounter();
    if (record && frame_start) push(&series[perf_stage_frame], (double) (now - frame_start) * ticks_to_ms);
    frame_start = now;
}

void perf_note_input_change(void) {
    if (enabled && !input_change_start) input_change_start = SDL_GetPerformanceCounter();
}

void perf_note_poll(void) {
    if (!enabled || !last_present) return;
    push(&series[perf_stage_present_to_poll], elapsed_ms(last_present));
    last_present = 0;
}

void perf_note_batch(const unsigned frames) {
    if (!enabled) return;

    batch_iterations++;
    batch_frames += frames;
    if (frames > batch_frames_peak) batch_frames_peak = frames;
    if (frames > 1) batch_catchup++;
}

void perf_note_present(void) {
    if (!enabled) return;

    const uint64_t now = SDL_GetPerformanceCounter();
    if (input_change_start) {
        push(&series[perf_stage_input_present], (double) (now - input_change_start) * ticks_to_ms);
        input_change_start = 0;
    }

    last_present = now;
}

static double batch_mean_frames(void) {
    return batch_iterations ? (double) batch_frames / (double) batch_iterations : 0.0;
}

static double batch_catchup_percent(void) {
    return batch_iterations ? 100.0 * (double) batch_catchup / (double) batch_iterations : 0.0;
}

void perf_format_hud(char *buf, const size_t len, const double fps) {
    const perf_series *lag = &series[perf_stage_input_present];

    char lag_text[32];
    if (lag->count) {
        snprintf(lag_text, sizeof(lag_text), "%.2f/%.2f ms", mean(lag), percentile95(lag));
    } else {
        snprintf(lag_text, sizeof(lag_text), "n/a");
    }

    snprintf(
        buf, len,
        "%.2f FPS\nFrame %.2f/%.2f ms\nCore %.2f  Video %.2f ms\nDraw %.2f  Flip %.2f ms\nAudio %.2f ms  Lag %s\nIdle "
        "%.2f ms  Delay %.2f ms\nQueue %u ms",
        fps, mean(&series[perf_stage_frame]), percentile95(&series[perf_stage_frame]), mean(&series[perf_stage_core]),
        mean(&series[perf_stage_video]), mean(&series[perf_stage_present_draw]), mean(&series[perf_stage_present_flip]),
        mean(&series[perf_stage_audio_wait]), lag_text, mean(&series[perf_stage_present_to_poll]),
        mean(&series[perf_stage_frame_delay]), audio_bridge_queued_ms()
    );
}

int perf_export_trace(const char *path) {
    static const char *names[perf_stage_count] = {"frame",           "core",         "video",      "present",
                                                  "present_draw",    "present_flip", "audio_wait", "input_present",
                                                  "present_to_poll", "frame_delay"};

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fputs("stage,mean_ms,p95_ms,peak_ms,samples\n", f);
    for (int i = 0; i < perf_stage_count; i++)
        fprintf(
            f, "%s,%.4f,%.4f,%.4f,%u\n", names[i], mean(&series[i]), percentile95(&series[i]), peak(&series[i]),
            series[i].count
        );

    fputs("\nmetric,value\n", f);
    fprintf(f, "core_batch_mean_frames,%.4f\n", batch_mean_frames());
    fprintf(f, "core_batch_peak_frames,%u\n", batch_frames_peak);
    fprintf(f, "core_batch_catchup_percent,%.2f\n", batch_catchup_percent());
    fprintf(f, "refresh_hz,%.4f\n", (double) frame_pacer_get_refresh_hz());

    fclose(f);
    return 0;
}
