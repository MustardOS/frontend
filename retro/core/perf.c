#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <common/platform/display.h>
#include <common/runtime/log.h>
#include <common/runtime/init.h>
#include "perf.h"
#include "core.h"
#include "muxretro.h"
#include "../video/hw_render.h"
#include "../video/colour.h"
#include "../video/interframe_blend.h"
#include "../settings/settings.h"
#include "../ui/options.h"

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
static double observed_core_run_hz;
static double observed_emulation_fps;
static unsigned netplay_resynchronisations;
static unsigned netplay_queue_overflows;
static unsigned cheevo_cache_hits;
static unsigned cheevo_cache_misses;
static unsigned cheevo_cache_fallbacks;
static unsigned cheevo_queue_rejections;
static unsigned cheevo_preview_drops;
static uint64_t audio_dropped_baseline;
static uint64_t audio_recovery_frames_baseline;
static uint64_t audio_recovery_count_baseline;
static uint32_t audio_underrun_baseline;
static uint32_t audio_underrun_event_baseline;
static uint64_t audio_underrun_missing_frames_baseline;
static uint64_t audio_burst_recovery_baseline;
static uint32_t frame_time_clamp_baseline;
static uint64_t audio_batch_call_baseline;
static double observed_audio_rate_correction_percent;
static double observed_audio_rate_limit_percent;
static uint32_t audio_queue_min_ms;
static unsigned audio_queue_below_low_samples;
static unsigned audio_queue_empty_samples;
static unsigned excluded_interaction_frames;
static unsigned excluded_long_frames;
static unsigned frame_histogram[6];
static int exclude_current_frame;
static int hud_active;
static int capture_active;
static int capture_automatic;
static int enabled;

static double perf_target_hz(void);

static int read_runtime_value(const char *path, char *buf, const size_t len) {
    if (!buf || len == 0) return 0;
    buf[0] = '\0';

    FILE *stream = fopen(path, "r");
    if (!stream) return 0;
    const size_t read = fread(buf, 1, len - 1, stream);
    fclose(stream);
    buf[read] = '\0';
    for (size_t i = 0; i < read; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            buf[i] = '\0';
            break;
        }
        if (buf[i] == ',') buf[i] = ';';
    }
    return buf[0] != '\0';
}

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
    observed_core_run_hz = 0.0;
    observed_emulation_fps = 0.0;
    netplay_resynchronisations = 0;
    netplay_queue_overflows = 0;
    cheevo_cache_hits = 0;
    cheevo_cache_misses = 0;
    cheevo_cache_fallbacks = 0;
    cheevo_queue_rejections = 0;
    cheevo_preview_drops = 0;
    pending_present_timing = 0;
    audio_dropped_baseline = audio_bridge_dropped_frames();
    audio_recovery_frames_baseline = audio_bridge_latency_recovery_frames();
    audio_recovery_count_baseline = audio_bridge_latency_recovery_count();
    audio_underrun_baseline = audio_bridge_underrun_count();
    audio_underrun_event_baseline = audio_bridge_underrun_event_count();
    audio_underrun_missing_frames_baseline = audio_bridge_underrun_missing_frames();
    audio_burst_recovery_baseline = audio_bridge_pickles_burst_recovery_count();
    frame_time_clamp_baseline = environment_frame_time_clamp_count();
    audio_batch_call_baseline = audio_bridge_batch_calls();
    observed_audio_rate_correction_percent = 0.0;
    observed_audio_rate_limit_percent = 0.0;
    audio_queue_min_ms = UINT32_MAX;
    audio_queue_below_low_samples = 0;
    audio_queue_empty_samples = 0;
    excluded_interaction_frames = 0;
    excluded_long_frames = 0;
    memset(frame_histogram, 0, sizeof(frame_histogram));
    exclude_current_frame = 0;
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

    if (perf_external_stage_active())
        LOG_INFO(
            mux_module, "External stage library is mapped; overlay execution is %s",
            getenv("DISABLE_HW_OVERLAY") ? "delegated to Pickles" : "enabled"
        );

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

double perf_stage_mean_ms(const enum perf_stage stage) {
    return (unsigned) stage < perf_stage_count ? mean(&series[stage]) : 0.0;
}

double perf_stage_p95_ms(const enum perf_stage stage) {
    return (unsigned) stage < perf_stage_count ? percentile95(&series[stage]) : 0.0;
}

int perf_external_stage_active(void) {
    static int cached = -1;
    if (cached >= 0) return cached;

    const char *preload = getenv("LD_PRELOAD");
    if (preload && strstr(preload, "libmustage")) return cached = 1;

    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) return cached = 0;

    char line[512];
    int active = 0;
    while (fgets(line, sizeof(line), maps)) {
        if (!strstr(line, "libmustage")) continue;
        active = 1;
        break;
    }
    fclose(maps);
    cached = active;
    return cached;
}

uint64_t perf_begin(void) {
    return enabled ? SDL_GetPerformanceCounter() : 0;
}

void perf_end(const enum perf_stage stage, const uint64_t start) {
    if (!enabled || !start || (unsigned) stage >= perf_stage_count) return;
    push(&series[stage], elapsed_ms(start));
    if (stage == perf_stage_screenshot || stage == perf_stage_state_save || stage == perf_stage_state_load)
        perf_exclude_current_frame(stage);

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

void perf_exclude_current_frame(const enum perf_stage reason) {
    if (!enabled) return;
    if (reason == perf_stage_screenshot || reason == perf_stage_state_save || reason == perf_stage_state_load)
        exclude_current_frame = 1;
}

static double perf_target_hz(void) {
    if (session_settings.fps_limit == fps_limit_50) return 50.0;

    if (session_settings.fps_limit == fps_limit_auto) {
        const double pace_ms = core_auto_pace_target_ms();
        if (pace_ms > 0.0) return 1000.0 / pace_ms;

        const double core_hz = core_get_target_fps();
        if (core_hz > 0.0) return core_hz;
    }

    const int panel = display_panel_refresh_hz();
    if (panel > 0) return panel;

    return frame_pacer_get_refresh_hz();
}

void perf_frame_complete(const int record) {
    if (!enabled) return;

    const uint64_t now = SDL_GetPerformanceCounter();
    if (record && frame_start && !exclude_current_frame) {
        const double frame_ms = (double) (now - frame_start) * ticks_to_ms;
        if (frame_ms > 250.0) {
            excluded_long_frames++;
        } else {
            push(&series[perf_stage_frame], frame_ms);
            frames_observed++;
            const unsigned bucket = frame_ms < 12.0   ? 0
                                    : frame_ms < 15.0 ? 1
                                    : frame_ms < 18.5 ? 2
                                    : frame_ms < 25.0 ? 3
                                    : frame_ms < 50.0 ? 4
                                                      : 5;
            frame_histogram[bucket]++;
            observed_audio_rate_correction_percent = audio_bridge_rate_correction_percent();
            observed_audio_rate_limit_percent = audio_bridge_rate_limit_percent();
            const uint32_t audio_queue_ms = audio_bridge_queued_ms();
            if (audio_queue_ms < audio_queue_min_ms) audio_queue_min_ms = audio_queue_ms;
            if (audio_queue_ms < audio_bridge_low_water_ms()) audio_queue_below_low_samples++;
            if (audio_queue_ms == 0) audio_queue_empty_samples++;

            const double target_hz = perf_target_hz();
            if (target_hz > 0.0 && frame_ms > 1000.0 / target_hz * 1.5) missed_refreshes++;
        }
    }
    if (record && exclude_current_frame) excluded_interaction_frames++;
    exclude_current_frame = 0;
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

void perf_note_rates(const double core_run_hz, const double emulation_fps) {
    if (!enabled) return;
    observed_core_run_hz = core_run_hz;
    observed_emulation_fps = emulation_fps;
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
        "%.2f FPS\nFrame %.2f/%.2f/%.2f ms\nCore %.2f  Video %.2f ms\nUpload %.2f ms\n"
        "Draw %.2f  Flip %.2f ms\nAudio %.2f ms  Lag "
        "%s\nIdle %.2f ms  Delay %.2f ms\nQueue %u ms  GL %.2f ms\nSvc %.2f  UI %.2f/%.2f ms\nMissed %u  "
        "Dupes %u\nSubmit %.2f  Rotate %.2f  Pace %.2f ms%s",
        fps, mean(&series[perf_stage_frame]), percentile95(&series[perf_stage_frame]),
        percentile99(&series[perf_stage_frame]), mean(&series[perf_stage_core]), mean(&series[perf_stage_video]),
        mean(&series[perf_stage_video_upload]), mean(&series[perf_stage_present_draw]),
        mean(&series[perf_stage_present_flip]), mean(&series[perf_stage_audio_wait]), lag_text,
        mean(&series[perf_stage_present_to_poll]), mean(&series[perf_stage_frame_delay]), audio_bridge_queued_ms(),
        gl_ms, mean(&series[perf_stage_services]), mean(&series[perf_stage_ui_logic]),
        mean(&series[perf_stage_ui_task]), missed_refreshes, video_duplicate_frames,
        mean(&series[perf_stage_gl_submit]), mean(&series[perf_stage_gl_rotate]), mean(&series[perf_stage_pace_sleep]),
        netplay_text
    );
}

int perf_export_trace(const char *path) {
    static const char *names[perf_stage_count] = {
        "frame",           "core",         "video",          "video_upload",       "present",
        "present_draw",    "present_flip", "audio_wait",     "audio_backpressure", "input_present",
        "present_to_poll", "frame_delay",  "gl_enter",       "gl_leave",           "gl_submit",
        "gl_rotate",       "pace_sleep",   "netplay_digest", "cheevo_callback",    "screenshot",
        "state_save",      "services",     "cheevo_tick",    "netplay_tick",       "maintenance",
        "control",         "ui_logic",     "ui_task",        "audio_queue",        "cheevo_frame",
        "anti_flicker",
        "texture_filter",
        "runahead_capture",
        "runahead_restore",
        "runahead_replay",
        "colour_pass",
        "state_load",
    };

    static const int parents[perf_stage_count] = {
        -1,
        -1,
        -1,
        perf_stage_core,
        -1,
        perf_stage_present,
        perf_stage_present,
        -1,
        perf_stage_core,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        perf_stage_cheevo_tick,
        -1,
        -1,
        -1,
        perf_stage_services,
        perf_stage_services,
        -1,
        -1,
        -1,
        -1,
        -1,
        -1,
        perf_stage_video_upload,
        perf_stage_video,
        -1,
        -1,
        -1,
        perf_stage_present,
        -1,
    };

    static const char *const scopes[perf_stage_count] = {
        "gameplay cadence",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "latency",
        "idle",
        "value",
        "work",
        "work",
        "work",
        "work",
        "wait",
        "work",
        "work",
        "interaction",
        "interaction",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "value",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "work",
        "interaction",
    };
    _Static_assert(sizeof(parents) / sizeof(parents[0]) == perf_stage_count, "perf stage parents are out of step");
    _Static_assert(sizeof(scopes) / sizeof(scopes[0]) == perf_stage_count, "perf stage scopes are out of step");

    _Static_assert(sizeof(names) / sizeof(names[0]) == perf_stage_count, "perf stage names are out of step");

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fputs("stage,parent,scope,mean_ms,exclusive_mean_ms,p50_ms,p95_ms,p99_ms,peak_ms,samples\n", f);
    for (int i = 0; i < perf_stage_count; i++) {
        double child_sum = 0.0;
        for (int child = 0; child < perf_stage_count; child++)
            if (parents[child] == i) child_sum += series[child].sum;
        double exclusive = series[i].count ? (series[i].sum - child_sum) / (double) series[i].count : 0.0;
        if (exclusive < 0.0) exclusive = 0.0;
        fprintf(
            f, "%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%u\n", names[i], parents[i] >= 0 ? names[parents[i]] : "none",
            scopes[i], mean(&series[i]), exclusive,
            percentile(&series[i], 50), percentile95(&series[i]), percentile99(&series[i]), peak(&series[i]),
            series[i].count
        );
    }

    fputs("\nframe_interval_ms\n", f);
    const perf_series *frames = &series[perf_stage_frame];
    const unsigned oldest = frames->count == PERF_HISTORY ? frames->next : 0;
    for (unsigned i = 0; i < frames->count; i++)
        fprintf(f, "%.4f\n", frames->samples[(oldest + i) % PERF_HISTORY]);

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
    fprintf(f, "observed_present_hz,%.4f\n", (double) frame_pacer_get_observed_hz());
    fprintf(f, "frames_observed,%u\n", frames_observed);
    fprintf(f, "frames_excluded_interaction,%u\n", excluded_interaction_frames);
    fprintf(f, "frames_excluded_long_gap,%u\n", excluded_long_frames);
    fprintf(f, "frame_histogram_under_12_ms,%u\n", frame_histogram[0]);
    fprintf(f, "frame_histogram_12_to_15_ms,%u\n", frame_histogram[1]);
    fprintf(f, "frame_histogram_15_to_18_5_ms,%u\n", frame_histogram[2]);
    fprintf(f, "frame_histogram_18_5_to_25_ms,%u\n", frame_histogram[3]);
    fprintf(f, "frame_histogram_25_to_50_ms,%u\n", frame_histogram[4]);
    fprintf(f, "frame_histogram_50_to_250_ms,%u\n", frame_histogram[5]);
    fprintf(f, "process_id,%ld\n", (long) getpid());
    fprintf(f, "parent_process_id,%ld\n", (long) getppid());
    char runtime_value[256];
    char runtime_path[64];
    snprintf(runtime_path, sizeof(runtime_path), "/proc/%ld/comm", (long) getppid());
    fprintf(
        f, "parent_process_name,%s\n",
        read_runtime_value(runtime_path, runtime_value, sizeof(runtime_value)) ? runtime_value : "unknown"
    );
    fprintf(
        f, "cpu_governor,%s\n",
        read_runtime_value(
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", runtime_value, sizeof(runtime_value)
        )
            ? runtime_value
            : "unknown"
    );
    fprintf(
        f, "cpu_frequency_khz,%s\n",
        read_runtime_value(
            "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", runtime_value, sizeof(runtime_value)
        )
            ? runtime_value
            : "unknown"
    );
    fprintf(f, "sdl_video_driver,%s\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "unknown");
    fprintf(f, "sdl_audio_driver,%s\n", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "unknown");
    SDL_RendererInfo renderer_info = {0};
    SDL_Renderer *renderer = display_get_renderer();
    if (renderer && SDL_GetRendererInfo(renderer, &renderer_info) == 0)
        fprintf(f, "sdl_renderer,%s\n", renderer_info.name ? renderer_info.name : "unknown");
    int window_w = 0, window_h = 0;
    SDL_Window *window = display_get_window();
    if (window) SDL_GetWindowSize(window, &window_w, &window_h);
    fprintf(f, "display_window_resolution,%dx%d\n", window_w, window_h);
    static const char *const environment_names[] = {
        "SDL_VIDEODRIVER", "SDL_RENDER_DRIVER", "SDL_AUDIODRIVER", "EGL_PLATFORM"
    };
    for (size_t i = 0; i < sizeof(environment_names) / sizeof(environment_names[0]); i++) {
        const char *value = getenv(environment_names[i]);
        fprintf(f, "env_%s,%s\n", environment_names[i], value && *value ? value : "unset");
    }
    const char *preload = getenv("LD_PRELOAD");
    fprintf(f, "external_stage_preload,%d\n", preload && strstr(preload, "libmustage") != NULL);
    fprintf(f, "external_stage_mapped,%d\n", perf_external_stage_active());
    const char *stage_disabled = getenv("DISABLE_HW_OVERLAY");
    fprintf(f, "external_stage_disabled,%d\n", stage_disabled && *stage_disabled == '1');
    fprintf(f, "core_run_hz,%.4f\n", observed_core_run_hz);
    fprintf(f, "emulation_fps,%.4f\n", observed_emulation_fps);
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
    const uint64_t total_recovery_frames = audio_bridge_latency_recovery_frames();
    const uint64_t recovery_frames = total_recovery_frames > audio_recovery_frames_baseline
                                         ? total_recovery_frames - audio_recovery_frames_baseline
                                         : 0;
    const uint64_t total_recoveries = audio_bridge_latency_recovery_count();
    const uint64_t recoveries =
        total_recoveries > audio_recovery_count_baseline ? total_recoveries - audio_recovery_count_baseline : 0;
    const uint32_t total_underruns = audio_bridge_underrun_count();
    const uint32_t audio_underruns =
        total_underruns > audio_underrun_baseline ? total_underruns - audio_underrun_baseline : 0;
    const uint32_t total_underrun_events = audio_bridge_underrun_event_count();
    const uint32_t audio_underrun_events = total_underrun_events > audio_underrun_event_baseline
                                               ? total_underrun_events - audio_underrun_event_baseline
                                               : 0;
    const uint64_t total_missing_frames = audio_bridge_underrun_missing_frames();
    const uint64_t audio_underrun_missing_frames = total_missing_frames > audio_underrun_missing_frames_baseline
                                                       ? total_missing_frames - audio_underrun_missing_frames_baseline
                                                       : 0;
    const uint64_t total_burst_recoveries = audio_bridge_pickles_burst_recovery_count();
    const uint64_t audio_burst_recoveries = total_burst_recoveries > audio_burst_recovery_baseline
                                                ? total_burst_recoveries - audio_burst_recovery_baseline
                                                : 0;

    fprintf(f, "audio_low_water_ms,%u\n", audio_bridge_low_water_ms());
    fprintf(f, "audio_high_water_ms,%u\n", audio_bridge_high_water_ms());
    fprintf(f, "audio_latency_floor_ms,%u\n", audio_bridge_latency_floor_ms());
    fprintf(f, "audio_prefill_target_ms,%u\n", audio_bridge_prefill_target_ms());
    fprintf(f, "audio_burst_reservoir_ms,%u\n", audio_bridge_burst_reservoir_ms());
    fprintf(f, "audio_backpressure_ceiling_ms,%u\n", audio_bridge_backpressure_ceiling_ms());
    fprintf(f, "audio_dropped_frames,%llu\n", (unsigned long long) audio_dropped);
    fprintf(f, "audio_dropped_seconds,%.3f\n", audio_freq > 0 ? (double) audio_dropped / (double) audio_freq : 0.0);
    fprintf(f, "audio_latency_recoveries,%llu\n", (unsigned long long) recoveries);
    fprintf(f, "audio_recovery_dropped_frames,%llu\n", (unsigned long long) recovery_frames);
    fprintf(
        f, "audio_recovery_dropped_seconds,%.3f\n",
        audio_freq > 0 ? (double) recovery_frames / (double) audio_freq : 0.0
    );
    fprintf(f, "audio_underruns,%u\n", audio_underruns);
    fprintf(f, "audio_underrun_callbacks,%u\n", audio_underruns);
    fprintf(f, "audio_underrun_events,%u\n", audio_underrun_events);
    fprintf(f, "audio_underrun_missing_frames,%llu\n", (unsigned long long) audio_underrun_missing_frames);
    fprintf(
        f, "audio_underrun_missing_seconds,%.3f\n",
        audio_freq > 0 ? (double) audio_underrun_missing_frames / (double) audio_freq : 0.0
    );
    fprintf(
        f, "audio_underrun_callbacks_per_event,%.2f\n",
        audio_underrun_events ? (double) audio_underruns / (double) audio_underrun_events : 0.0
    );
    fprintf(f, "audio_queue_min_ms,%u\n", audio_queue_min_ms == UINT32_MAX ? 0 : audio_queue_min_ms);
    fprintf(f, "audio_queue_below_low_samples,%u\n", audio_queue_below_low_samples);
    fprintf(
        f, "audio_queue_below_low_percent,%.2f\n",
        frames_observed ? 100.0 * (double) audio_queue_below_low_samples / (double) frames_observed : 0.0
    );
    fprintf(f, "audio_queue_empty_samples,%u\n", audio_queue_empty_samples);
    fprintf(f, "audio_burst_recoveries,%llu\n", (unsigned long long) audio_burst_recoveries);
    fprintf(f, "audio_burst_recovery_active,%d\n", audio_bridge_pickles_burst_recovery_active());
    fprintf(f, "audio_burst_recovery_peak_percent,%.4f\n", audio_bridge_pickles_burst_recovery_peak_percent());
    fprintf(f, "audio_rate_correction_percent,%.4f\n", observed_audio_rate_correction_percent);
    fprintf(f, "audio_rate_limit_percent,%.4f\n", observed_audio_rate_limit_percent);
    fprintf(
        f, "audio_batch_calls,%llu\n", (unsigned long long) (audio_bridge_batch_calls() - audio_batch_call_baseline)
    );
    fprintf(f, "audio_batch_peak_frames,%zu\n", audio_bridge_batch_peak_frames());
    fprintf(f, "content_hz,%.4f\n", audio_bridge_content_fps());
    fprintf(f, "content_locked_hz,%.4f\n", audio_bridge_locked_content_fps());
    fprintf(f, "content_quantum_hz,%.4f\n", audio_bridge_content_quantum_fps());
    fprintf(f, "content_paced,%d\n", core_content_needs_pacing());
    fprintf(f, "audio_pace_target_ms,%.4f\n", core_auto_pace_target_ms());
    fprintf(f, "paced_target_hz,%.4f\n", perf_target_hz());
    fprintf(f, "core_target_hz,%.4f\n", core_get_target_fps());
    fprintf(f, "core_pace_divisor,%.4f\n", core_pace_divisor());
    fprintf(f, "panel_hz,%d\n", display_panel_refresh_hz());
    fprintf(f, "fps_limit_mode,%d\n", session_settings.fps_limit);
    fprintf(f, "gpu_hard_sync,%d\n", session_settings.gpu_hard_sync);
    fprintf(f, "swap_interval,%d\n", video_bridge_get_swap_interval());
    fprintf(f, "frame_time_callback,%d\n", environment_frame_time_callback_active());
    fprintf(f, "frame_time_clamps,%u\n", environment_frame_time_clamp_count() - frame_time_clamp_baseline);
    fprintf(f, "frame_time_clamp_peak_ms,%.4f\n", environment_frame_time_clamp_peak_ms());

    int source_w = 0, source_h = 0;
    int logical_w = 0, logical_h = 0;
    int output_w = 0, output_h = 0;
    int integer_mapped = 0;
    video_bridge_get_output_geometry(
        &source_w, &source_h, &logical_w, &logical_h, &output_w, &output_h, &integer_mapped
    );
    fprintf(f, "video_source_resolution,%dx%d\n", source_w, source_h);
    fprintf(f, "video_logical_resolution,%dx%d\n", logical_w, logical_h);
    fprintf(f, "video_output_resolution,%dx%d\n", output_w, output_h);
    fprintf(f, "video_output_scale_x,%.6f\n", source_w > 0 ? (double) output_w / (double) source_w : 0.0);
    fprintf(f, "video_output_scale_y,%.6f\n", source_h > 0 ? (double) output_h / (double) source_h : 0.0);
    fprintf(f, "video_output_integer_mapped,%d\n", integer_mapped);
    fprintf(f, "anti_flicker_available,%d\n", video_bridge_anti_flicker_available());
    fprintf(f, "anti_flicker_history_bytes,%zu\n", video_bridge_anti_flicker_bytes());
    fprintf(f, "anti_flicker_last_ms,%.4f\n", interframe_blend_last_ms());
    fprintf(f, "anti_flicker_threads,%u\n", interframe_blend_thread_count());
    fprintf(f, "anti_flicker_thread_threshold_pixels,%zu\n", interframe_blend_thread_threshold_pixels());
    fprintf(f, "cpu_texture_filter_active,%d\n", video_bridge_cpu_filter_active());
    fprintf(f, "shimmer_fix,%d\n", session_settings.shimmer_fix);
    fprintf(f, "viewport_zoom,%d\n", session_settings.viewport_zoom);
    fprintf(f, "viewport_stretch_x,%d\n", session_settings.viewport_stretch_x);
    fprintf(f, "viewport_stretch_y,%d\n", session_settings.viewport_stretch_y);
    fprintf(f, "viewport_crop_left,%d\n", session_settings.viewport_crop_left);
    fprintf(f, "viewport_crop_right,%d\n", session_settings.viewport_crop_right);
    fprintf(f, "viewport_crop_top,%d\n", session_settings.viewport_crop_top);
    fprintf(f, "viewport_crop_bottom,%d\n", session_settings.viewport_crop_bottom);
    colour_shader_export_contract(f);

    const char *gl_context = "none";
    if (hw_render_bridge_active()) gl_context = hw_render_bridge_owns_context() ? "dedicated" : "shared";
    fprintf(f, "gl_context,%s\n", gl_context);
    const char *gl_backend = hw_render_bridge_description();
    fprintf(f, "gl_backend,%s\n", gl_backend ? gl_backend : "none");
    fprintf(f, "gl_buffers,%d\n", hw_render_bridge_buffer_count());

    char core_name[128] = "unknown";
    core_get_name(core_file_path, core_name, sizeof(core_name));
    fprintf(f, "core_name,%s\n", core_name);
    struct retro_system_info core_info = {0};
    fprintf(
        f, "core_version,%s\n",
        core_cached_system_info(&core_info) && core_info.library_version ? core_info.library_version : "unknown"
    );

    static const char *const ppsspp_options[] = {
        "ppsspp_cpu_core",
        "ppsspp_fast_memory",
        "ppsspp_io_timing_method",
        "ppsspp_internal_resolution",
        "ppsspp_pickles_output_upscaler",
        "ppsspp_frameskip",
        "ppsspp_frameskiptype",
        "ppsspp_auto_frameskip",
        "ppsspp_frame_duplication",
        "ppsspp_detect_vsync_swap_interval",
        "ppsspp_inflight_frames",
        "ppsspp_skip_buffer_effects",
        "ppsspp_skip_gpu_readbacks",
        "ppsspp_lazy_texture_caching",
        "ppsspp_spline_quality",
        "ppsspp_gpu_hardware_transform",
        "ppsspp_software_skinning",
        "ppsspp_lower_resolution_for_effects",
        "ppsspp_texture_scaling_level",
        "ppsspp_texture_anisotropic_filtering",
    };
    for (size_t i = 0; i < sizeof(ppsspp_options) / sizeof(ppsspp_options[0]); i++) {
        const char *value = options_get_value(ppsspp_options[i]);
        if (value) fprintf(f, "%s,%s\n", ppsspp_options[i], value);
    }
    const char *pickles_upscaler = options_get_value("ppsspp_pickles_output_upscaler");
    if (pickles_upscaler && strcmp(pickles_upscaler, "Sharp Bilinear") == 0)
        fprintf(f, "ppsspp_pickles_effective_scene_resolution,480x272\n");

    const char *threaded_rendering = options_get_value("reicast_threaded_rendering");
    const char *auto_skip_frame = options_get_value("reicast_auto_skip_frame");
    const char *detect_vsync_swap_interval = options_get_value("reicast_detect_vsync_swap_interval");
    const char *anisotropic_filtering = options_get_value("reicast_anisotropic_filtering");
    const char *synchronous_rendering = options_get_value("reicast_synchronous_rendering");
    const char *delay_frame_swapping = options_get_value("reicast_delay_frame_swapping");
    const char *frame_skipping = options_get_value("reicast_frame_skipping");
    const char *framerate = options_get_value("reicast_framerate");
    const char *alpha_sorting = options_get_value("reicast_alpha_sorting");
    const char *enable_dsp = options_get_value("reicast_enable_dsp");
    const char *broadcast = options_get_value("reicast_broadcast");
    const char *cable_type = options_get_value("reicast_cable_type");
    if (threaded_rendering) fprintf(f, "flycast_threaded_rendering,%s\n", threaded_rendering);
    if (auto_skip_frame) fprintf(f, "flycast_auto_skip_frame,%s\n", auto_skip_frame);
    if (detect_vsync_swap_interval) fprintf(f, "flycast_detect_vsync_swap_interval,%s\n", detect_vsync_swap_interval);
    if (anisotropic_filtering) fprintf(f, "flycast_anisotropic_filtering,%s\n", anisotropic_filtering);
    if (synchronous_rendering) fprintf(f, "flycast_synchronous_rendering,%s\n", synchronous_rendering);
    if (delay_frame_swapping) fprintf(f, "flycast_delay_frame_swapping,%s\n", delay_frame_swapping);
    if (frame_skipping) fprintf(f, "flycast_frame_skipping,%s\n", frame_skipping);
    if (framerate) fprintf(f, "flycast_framerate,%s\n", framerate);
    if (alpha_sorting) fprintf(f, "flycast_alpha_sorting,%s\n", alpha_sorting);
    if (enable_dsp) fprintf(f, "flycast_enable_dsp,%s\n", enable_dsp);
    if (broadcast) fprintf(f, "flycast_broadcast,%s\n", broadcast);
    if (cable_type) fprintf(f, "flycast_cable_type,%s\n", cable_type);

    fclose(f);
    return 0;
}
