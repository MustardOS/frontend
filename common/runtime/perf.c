#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <common/runtime/perf.h>
#include <common/config/config.h>
#include <common/storage/fileio.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/base/options.h>
#include <common/platform/device.h>
#include <common/platform/display.h>

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
static unsigned saver_draw_calls;
static unsigned saver_deadline_misses;
static char saver_name[48];
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
    saver_draw_calls = 0;
    saver_deadline_misses = 0;
    saver_name[0] = '\0';
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

void fe_perf_set_saver(const char *name) {
    if (!enabled) return;
    snprintf(saver_name, sizeof(saver_name), "%s", name ? name : "");
}

void fe_perf_note_saver_draw_calls(const unsigned count) {
    if (enabled) saver_draw_calls += count;
}

void fe_perf_note_saver_deadline_miss(const unsigned count) {
    if (enabled) saver_deadline_misses += count;
}

int fe_perf_export_trace(const char *path) {
    static const char *names[fe_perf_stage_count] = {
        "loop", "input", "nav", "list", "catalogue", "image", "glyph", "font", "lv_task", "render", "idle",
        "saver_update", "saver_render", "saver_present", "saver_scan", "saver_decode",
    };
    _Static_assert(sizeof(names) / sizeof(names[0]) == fe_perf_stage_count, "perf stage names are out of step");

    create_directories(STORAGE_PERF, 0);

    const int fresh = !file_exist(path);
    FILE *f = fopen(path, "a");
    if (!f) {
        LOG_ERROR(mux_module, "Could not write performance counters to %s", path);
        return -1;
    }

    if (fresh)
        fputs(
            "module,context,stage,mean_ms,p50_ms,p95_ms,p99_ms,peak_ms,samples,loops,draw_calls,deadline_misses\n", f
        );

    for (int i = 0; i < fe_perf_stage_count; i++) {
        if (!series[i].count) continue;

        fprintf(
            f, "%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%u,%u,%u,%u\n", mux_module, saver_name[0] ? saver_name : "ui",
            names[i], mean(&series[i]), percentile(&series[i], 50), percentile(&series[i], 95), percentile(&series[i], 99), peak(&series[i]),
            series[i].count, loop_count, saver_draw_calls,
            saver_deadline_misses
        );
    }

    const int ok = fflush(f) == 0;
    fclose(f);

    if (ok) LOG_INFO(mux_module, "Performance counters appended to %s", path);
    return ok ? 0 : -1;
}

int fe_perf_export_support(const char *path) {
    create_directories(STORAGE_PERF, 0);

    char temp[MAX_BUFFER_SIZE];
    const int written = snprintf(temp, sizeof(temp), "%s.tmp", path);
    if (written < 0 || (size_t) written >= sizeof(temp)) return -1;

    FILE *f = fopen(temp, "w");
    if (!f) return -1;

    SDL_RendererInfo renderer = {0};
    SDL_Renderer *active_renderer = display_get_renderer();
    if (active_renderer) SDL_GetRendererInfo(active_renderer, &renderer);

    long rss_kb = -1;
    FILE *status = fopen("/proc/self/status", "r");
    if (status) {
        char line[160];
        while (fgets(line, sizeof(line), status)) {
            if (sscanf(line, "VmRSS: %ld kB", &rss_kb) == 1) break;
        }
        fclose(status);
    }

    fputs("MustardOS frontend support context\n", f);
    fprintf(f, "module=%s\n", mux_module);
    fprintf(f, "version=%s\n", config.system.version);
    fprintf(f, "build=%s\n", config.system.build);
    fprintf(f, "device=%s\n", device.board.name);
    fprintf(f, "display=%dx%d\n", device.mux.width, device.mux.height);
    fprintf(f, "panel_refresh_hz=%d\n", display_panel_refresh_hz());
    fprintf(f, "renderer=%s\n", renderer.name ? renderer.name : "unknown");
    fprintf(f, "renderer_max_texture=%dx%d\n", renderer.max_texture_width, renderer.max_texture_height);
    fprintf(f, "rss_kb=%ld\n", rss_kb);
    fprintf(f, "theme=%s\n", config.theme.active);
    fprintf(f, "motion_mode=%d\n", config.visual.reduce_motion);
    fprintf(f, "render_shadows=%d\n", config.visual.render_shadows);
    fprintf(f, "video_wallpaper=%d\n", config.visual.video_wallpaper);
    fprintf(f, "video_preview=%d\n", config.visual.video_preview);
    fprintf(f, "double_buffer=%d\n", config.settings.advanced.double_buffer);
    fprintf(f, "capture_enabled=%d\n", enabled);
    fprintf(f, "saver=%s\n", saver_name[0] ? saver_name : "none");
    fprintf(f, "saver_draw_calls=%u\n", saver_draw_calls);
    fprintf(f, "saver_deadline_misses=%u\n", saver_deadline_misses);
    fputs("performance_trace=MUOS/performance/frontend.csv\n", f);
    fputs(
        "privacy=No content names, content paths, network names, addresses, credentials, or serial identifiers "
        "included.\n",
        f
    );

    const int flushed = fflush(f) == 0;
    const int closed = fclose(f) == 0;
    const int ok = flushed && closed && rename(temp, path) == 0;
    if (!ok) remove(temp);
    return ok ? 0 : -1;
}

void fe_perf_flush(void) {
    if (!enabled) return;

    if (series[fe_perf_stage_loop].count || series[fe_perf_stage_saver_render].count)
        fe_perf_export_trace(STORAGE_PERF "/frontend.csv");
    if (fe_perf_export_support(STORAGE_PERF "/frontend-support.txt") != 0)
        LOG_ERROR(mux_module, "Could not write frontend support context");

    enabled = 0;
    capture_active = 0;
}
