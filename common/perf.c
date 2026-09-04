#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "perf.h"
#include "config.h"
#include "fileio.h"
#include "init.h"
#include "log.h"
#include "options.h"

#define FE_PERF_HISTORY 512

typedef struct {
    float samples[FE_PERF_HISTORY];
    unsigned count;
    unsigned next;
    double sum;
} fe_perf_series;

static fe_perf_series series[fe_perf_stage_count];
static double ticks_to_ms;
static unsigned loop_count;
static int capture_active;
static int enabled;

static double elapsed_ms(const uint64_t start) {
    return (double) (SDL_GetPerformanceCounter() - start) * ticks_to_ms;
}

static void push(fe_perf_series *s, const double ms) {
    if (s->count == FE_PERF_HISTORY) {
        s->sum -= s->samples[s->next];
    } else {
        s->count++;
    }

    s->samples[s->next] = (float) ms;
    s->sum += ms;
    s->next = (s->next + 1) % FE_PERF_HISTORY;
}

static double mean(const fe_perf_series *s) {
    return s->count ? s->sum / (double) s->count : 0.0;
}

static double peak(const fe_perf_series *s) {
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

static double percentile(const fe_perf_series *s, const unsigned rank) {
    if (!s->count) return 0.0;

    static float sorted[FE_PERF_HISTORY];
    memcpy(sorted, s->samples, sizeof(float) * s->count);
    qsort(sorted, s->count, sizeof(float), compare_sample);

    return sorted[(s->count - 1) * rank / 100];
}

static void reset(void) {
    memset(series, 0, sizeof(series));
    loop_count = 0;
}

void fe_perf_init(void) {
    ticks_to_ms = 1000.0 / (double) SDL_GetPerformanceFrequency();
    capture_active = 0;
    enabled = 0;
    reset();

    if (config.settings.advanced.perf_counters) fe_perf_set_capture_active(1);
}

void fe_perf_set_capture_active(const int active) {
    capture_active = !!active;
    enabled = capture_active;
    if (enabled) reset();
}

int fe_perf_is_capture_active(void) {
    return capture_active;
}

int fe_perf_is_enabled(void) {
    return enabled;
}

uint64_t fe_perf_begin(void) {
    return enabled ? SDL_GetPerformanceCounter() : 0;
}

void fe_perf_end(const enum fe_perf_stage stage, const uint64_t start) {
    if (!enabled || !start || (unsigned) stage >= fe_perf_stage_count) return;
    push(&series[stage], elapsed_ms(start));
}

void fe_perf_record(const enum fe_perf_stage stage, const double ms) {
    if (!enabled || (unsigned) stage >= fe_perf_stage_count) return;
    push(&series[stage], ms);
}

void fe_perf_loop_complete(void) {
    if (!enabled) return;
    loop_count++;
}

int fe_perf_export_trace(const char *path) {
    static const char *names[fe_perf_stage_count] = {
        "loop", "input", "nav", "list", "catalogue", "image", "glyph", "font", "lv_task", "render", "idle",
    };
    _Static_assert(sizeof(names) / sizeof(names[0]) == fe_perf_stage_count, "perf stage names are out of step");

    create_directories(STORAGE_PERF, 0);

    const int fresh = !file_exist(path);
    FILE *f = fopen(path, "a");
    if (!f) {
        LOG_ERROR(mux_module, "Could not write performance counters to %s", path);
        return -1;
    }

    if (fresh) fputs("module,stage,mean_ms,p50_ms,p95_ms,p99_ms,peak_ms,samples,loops\n", f);

    for (int i = 0; i < fe_perf_stage_count; i++) {
        if (!series[i].count) continue;

        fprintf(
            f, "%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u\n", mux_module, names[i], mean(&series[i]),
            percentile(&series[i], 50), percentile(&series[i], 95), percentile(&series[i], 99), peak(&series[i]),
            series[i].count, loop_count
        );
    }

    const int ok = fflush(f) == 0;
    fclose(f);

    if (ok) LOG_INFO(mux_module, "Performance counters appended to %s", path);
    return ok ? 0 : -1;
}

void fe_perf_flush(void) {
    if (!enabled) return;

    if (series[fe_perf_stage_loop].count) fe_perf_export_trace(STORAGE_PERF "/frontend.csv");

    enabled = 0;
    capture_active = 0;
}
