#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/display.h"
#include "../../common/log.h"
#include "../../common/init.h"
#include "perf.h"
#include "muxretro.h"
#include "../video/hw_render.h"

#define PERF_HISTORY 1024

enum netplay_metric {
    netplay_metric_tx_queue = 0,
    netplay_metric_rx_queue,
    netplay_metric_input_age,
    netplay_metric_state_jobs,
    netplay_metric_digest_jobs,
    netplay_metric_ping,
    netplay_metric_jitter,
    netplay_metric_count
};

enum cheevo_metric {
    cheevo_metric_request_queue = 0,
    cheevo_metric_completion_queue,
    cheevo_metric_preview_queue,
    cheevo_metric_oldest_job_ms,
    cheevo_metric_count
};

typedef struct {
    float samples[PERF_HISTORY];
    unsigned next;
    unsigned count;
    double sum;
} perf_series;

static perf_series series[perf_stage_count];
static perf_series netplay_series[netplay_metric_count];
static perf_series cheevo_series[cheevo_metric_count];
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
static unsigned missed_refreshes;
static unsigned frames_observed;
static unsigned video_frames;
static unsigned video_duplicate_frames;
static unsigned netplay_resynchronisations;
static unsigned netplay_queue_overflows;
static unsigned cheevo_cache_hits;
static unsigned cheevo_cache_misses;
static unsigned cheevo_cache_fallbacks;
static unsigned cheevo_queue_rejections;
static unsigned cheevo_preview_drops;
static uint64_t audio_dropped_baseline;
static int hud_active;
static int capture_active;
static int capture_automatic;
static int enabled;

static double perf_target_hz(void);

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

static double percentile(const perf_series *s, const unsigned rank) {
    if (!s->count) return 0.0;

    static float sorted[PERF_HISTORY];
    memcpy(sorted, s->samples, sizeof(float) * s->count);
    qsort(sorted, s->count, sizeof(float), compare_sample);

    return sorted[(s->count - 1) * rank / 100];
}

static double percentile95(const perf_series *s) {
    return percentile(s, 95);
}

static double percentile99(const perf_series *s) {
    return percentile(s, 99);
}

static void reset(void) {
    memset(series, 0, sizeof(series));
    memset(netplay_series, 0, sizeof(netplay_series));
    memset(cheevo_series, 0, sizeof(cheevo_series));
    frame_start = SDL_GetPerformanceCounter();
    input_change_start = 0;
    last_present = 0;
    batch_iterations = 0;
    batch_frames = 0;
    batch_frames_peak = 0;
    batch_catchup = 0;
    missed_refreshes = 0;
    frames_observed = 0;
    video_frames = 0;
    video_duplicate_frames = 0;
    netplay_resynchronisations = 0;
    netplay_queue_overflows = 0;
    cheevo_cache_hits = 0;
    cheevo_cache_misses = 0;
    cheevo_cache_fallbacks = 0;
    cheevo_queue_rejections = 0;
    cheevo_preview_drops = 0;
    pending_present_timing = 0;
    audio_dropped_baseline = audio_bridge_dropped_frames();
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

    const char *env = getenv("MUXRETRO_PERF_CAPTURE");
    if (env && *env == '1') {
        perf_set_capture_active(1);
        capture_automatic = 1;
        LOG_INFO(mux_module, "Performance capture armed by MUXRETRO_PERF_CAPTURE");
    }
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

int perf_capture_is_automatic(void) {
    return capture_automatic;
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

static double perf_target_hz(void) {
    if (core_content_needs_pacing()) {
        const double locked = audio_bridge_locked_content_fps();
        if (locked > 0.0) return locked;
    }

    const int panel = display_panel_refresh_hz();
    if (panel > 0) return panel;

    return frame_pacer_get_refresh_hz();
}

void perf_frame_complete(const int record) {
    if (!enabled) return;

    const uint64_t now = SDL_GetPerformanceCounter();
    if (record && frame_start) {
        const double frame_ms = (double) (now - frame_start) * ticks_to_ms;
        push(&series[perf_stage_frame], frame_ms);
        frames_observed++;

        const double target_hz = perf_target_hz();
        if (target_hz > 0.0 && frame_ms > 1000.0 / target_hz * 1.5) missed_refreshes++;
    }
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

void perf_note_video_frame(const int duplicate) {
    if (!enabled) return;

    video_frames++;
    if (duplicate) video_duplicate_frames++;
}

void perf_note_netplay(const perf_netplay_snapshot *snapshot) {
    if (!enabled || !snapshot) return;

    push(&netplay_series[netplay_metric_tx_queue], snapshot->tx_queue);
    push(&netplay_series[netplay_metric_rx_queue], snapshot->rx_queue);
    push(&netplay_series[netplay_metric_input_age], snapshot->input_age_frames);
    push(&netplay_series[netplay_metric_state_jobs], snapshot->state_jobs);
    push(&netplay_series[netplay_metric_digest_jobs], snapshot->digest_jobs);
    push(&netplay_series[netplay_metric_ping], snapshot->ping_ms);
    push(&netplay_series[netplay_metric_jitter], snapshot->jitter_ms);
    netplay_resynchronisations = snapshot->resynchronisations;
    netplay_queue_overflows = snapshot->queue_overflows;
}

void perf_note_cheevo(const perf_cheevo_snapshot *snapshot) {
    if (!enabled || !snapshot) return;

    push(&cheevo_series[cheevo_metric_request_queue], snapshot->request_queue);
    push(&cheevo_series[cheevo_metric_completion_queue], snapshot->completion_queue);
    push(&cheevo_series[cheevo_metric_preview_queue], snapshot->preview_queue);
    push(&cheevo_series[cheevo_metric_oldest_job_ms], snapshot->oldest_job_ms);
    cheevo_cache_hits = snapshot->cache_hits;
    cheevo_cache_misses = snapshot->cache_misses;
    cheevo_cache_fallbacks = snapshot->cache_fallbacks;
    cheevo_queue_rejections = snapshot->queue_rejections;
    cheevo_preview_drops = snapshot->preview_drops;
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

    const double gl_ms = mean(&series[perf_stage_gl_enter]) + mean(&series[perf_stage_gl_leave]);

    char netplay_text[96] = "";
    if (netplay_series[netplay_metric_tx_queue].count) {
        snprintf(
            netplay_text, sizeof(netplay_text), "\nNet Q %.1f/%.1f  Age %.1f f  Ping %.1f ms",
            mean(&netplay_series[netplay_metric_tx_queue]), mean(&netplay_series[netplay_metric_rx_queue]),
            mean(&netplay_series[netplay_metric_input_age]), mean(&netplay_series[netplay_metric_ping])
        );
    }

    snprintf(
        buf, len,
        "%.2f FPS\nFrame %.2f/%.2f/%.2f ms\nCore %.2f  Video %.2f ms\nDraw %.2f  Flip %.2f ms\nAudio %.2f ms  Lag "
        "%s\nIdle %.2f ms  Delay %.2f ms\nQueue %u ms  GL %.2f ms\nSvc %.2f  UI %.2f/%.2f ms\nMissed %u  "
        "Dupes %u%s",
        fps, mean(&series[perf_stage_frame]), percentile95(&series[perf_stage_frame]),
        percentile99(&series[perf_stage_frame]), mean(&series[perf_stage_core]), mean(&series[perf_stage_video]),
        mean(&series[perf_stage_present_draw]), mean(&series[perf_stage_present_flip]),
        mean(&series[perf_stage_audio_wait]), lag_text, mean(&series[perf_stage_present_to_poll]),
        mean(&series[perf_stage_frame_delay]), audio_bridge_queued_ms(), gl_ms, mean(&series[perf_stage_services]),
        mean(&series[perf_stage_ui_logic]), mean(&series[perf_stage_ui_task]), missed_refreshes, video_duplicate_frames,
        netplay_text
    );
}

int perf_export_trace(const char *path) {
    static const char *names[perf_stage_count] = {
        "frame",        "core",       "video",          "present",         "present_draw",
        "present_flip", "audio_wait", "input_present",  "present_to_poll", "frame_delay",
        "gl_enter",     "gl_leave",   "netplay_digest", "cheevo_callback", "screenshot",
        "state_save",   "services",   "cheevo_tick",    "netplay_tick",    "maintenance",
        "control",      "ui_logic",   "ui_task",        "audio_queue",     "cheevo_frame"
    };

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fputs("stage,mean_ms,p50_ms,p95_ms,p99_ms,peak_ms,samples\n", f);
    for (int i = 0; i < perf_stage_count; i++)
        fprintf(
            f, "%s,%.4f,%.4f,%.4f,%.4f,%.4f,%u\n", names[i], mean(&series[i]), percentile(&series[i], 50),
            percentile95(&series[i]), percentile99(&series[i]), peak(&series[i]), series[i].count
        );

    static const char *netplay_names[netplay_metric_count] = {"tx_queue_packets", "rx_queue_packets",
                                                              "input_age_frames", "state_jobs",
                                                              "digest_jobs",      "ping_ms",
                                                              "jitter_ms"};
    fputs("\nnetplay_metric,mean,p50,p95,p99,peak,samples\n", f);
    for (int i = 0; i < netplay_metric_count; i++)
        fprintf(
            f, "%s,%.4f,%.4f,%.4f,%.4f,%.4f,%u\n", netplay_names[i], mean(&netplay_series[i]),
            percentile(&netplay_series[i], 50), percentile95(&netplay_series[i]), percentile99(&netplay_series[i]),
            peak(&netplay_series[i]), netplay_series[i].count
        );

    static const char *cheevo_names[cheevo_metric_count] = {
        "request_queue", "completion_queue", "preview_queue", "oldest_job_ms"
    };
    fputs("\nachievement_metric,mean,p50,p95,p99,peak,samples\n", f);
    for (int i = 0; i < cheevo_metric_count; i++)
        fprintf(
            f, "%s,%.4f,%.4f,%.4f,%.4f,%.4f,%u\n", cheevo_names[i], mean(&cheevo_series[i]),
            percentile(&cheevo_series[i], 50), percentile95(&cheevo_series[i]), percentile99(&cheevo_series[i]),
            peak(&cheevo_series[i]), cheevo_series[i].count
        );

    fputs("\nmetric,value\n", f);
    fprintf(f, "core_batch_mean_frames,%.4f\n", batch_mean_frames());
    fprintf(f, "core_batch_peak_frames,%u\n", batch_frames_peak);
    fprintf(f, "core_batch_catchup_percent,%.2f\n", batch_catchup_percent());
    fprintf(f, "refresh_hz,%.4f\n", (double) frame_pacer_get_refresh_hz());
    fprintf(f, "frames_observed,%u\n", frames_observed);
    fprintf(f, "missed_refreshes,%u\n", missed_refreshes);
    fprintf(f, "video_frames,%u\n", video_frames);
    fprintf(f, "video_duplicate_frames,%u\n", video_duplicate_frames);
    fprintf(
        f, "video_duplicate_percent,%.2f\n",
        video_frames ? 100.0 * (double) video_duplicate_frames / (double) video_frames : 0.0
    );
    fprintf(f, "netplay_resynchronisations,%u\n", netplay_resynchronisations);
    fprintf(f, "netplay_queue_overflows,%u\n", netplay_queue_overflows);
    fprintf(f, "achievement_cache_hits,%u\n", cheevo_cache_hits);
    fprintf(f, "achievement_cache_misses,%u\n", cheevo_cache_misses);
    fprintf(f, "achievement_cache_fallbacks,%u\n", cheevo_cache_fallbacks);
    fprintf(
        f, "achievement_cache_hit_percent,%.2f\n",
        cheevo_cache_hits + cheevo_cache_misses
            ? 100.0 * (double) cheevo_cache_hits / (double) (cheevo_cache_hits + cheevo_cache_misses)
            : 0.0
    );
    fprintf(f, "achievement_queue_rejections,%u\n", cheevo_queue_rejections);
    fprintf(f, "achievement_preview_drops,%u\n", cheevo_preview_drops);
    fprintf(
        f, "missed_refresh_percent,%.2f\n",
        frames_observed ? 100.0 * (double) missed_refreshes / (double) frames_observed : 0.0
    );
    int audio_freq = 0, audio_channels = 0;
    audio_bridge_get_info(&audio_freq, &audio_channels);

    const uint64_t total_dropped = audio_bridge_dropped_frames();
    const uint64_t audio_dropped = total_dropped > audio_dropped_baseline ? total_dropped - audio_dropped_baseline : 0;

    fprintf(f, "audio_low_water_ms,%u\n", audio_bridge_low_water_ms());
    fprintf(f, "audio_high_water_ms,%u\n", audio_bridge_high_water_ms());
    fprintf(f, "audio_dropped_frames,%llu\n", (unsigned long long) audio_dropped);
    fprintf(f, "audio_dropped_seconds,%.3f\n", audio_freq > 0 ? (double) audio_dropped / (double) audio_freq : 0.0);
    fprintf(f, "content_hz,%.4f\n", audio_bridge_content_fps());
    fprintf(f, "content_locked_hz,%.4f\n", audio_bridge_locked_content_fps());
    fprintf(f, "content_paced,%d\n", core_content_needs_pacing());
    fprintf(f, "paced_target_hz,%.4f\n", perf_target_hz());
    fprintf(f, "panel_hz,%d\n", display_panel_refresh_hz());

    const char *gl_context = "none";
    if (hw_render_bridge_active()) gl_context = hw_render_bridge_owns_context() ? "dedicated" : "shared";
    fprintf(f, "gl_context,%s\n", gl_context);

    fclose(f);
    return 0;
}
