#pragma once

#include <stddef.h>
#include <stdint.h>

enum perf_stage {
    perf_stage_frame = 0,
    perf_stage_core,
    perf_stage_video,
    perf_stage_present,
    perf_stage_present_draw,
    perf_stage_present_flip,
    perf_stage_audio_wait,
    perf_stage_input_present,
    perf_stage_present_to_poll,
    perf_stage_frame_delay,
    perf_stage_gl_enter,
    perf_stage_gl_leave,
    perf_stage_netplay_digest,
    perf_stage_cheevo_callback,
    perf_stage_screenshot,
    perf_stage_state_save,
    perf_stage_services,
    perf_stage_cheevo_tick,
    perf_stage_netplay_tick,
    perf_stage_maintenance,
    perf_stage_control,
    perf_stage_ui_logic,
    perf_stage_ui_task,
    perf_stage_count
};

typedef struct {
    unsigned tx_queue;
    unsigned rx_queue;
    unsigned input_age_frames;
    unsigned state_jobs;
    unsigned digest_jobs;
    unsigned ping_ms;
    unsigned jitter_ms;
    unsigned resynchronisations;
    unsigned queue_overflows;
} perf_netplay_snapshot;

typedef struct {
    unsigned request_queue;
    unsigned completion_queue;
    unsigned preview_queue;
    unsigned oldest_job_ms;
    unsigned cache_hits;
    unsigned cache_misses;
    unsigned cache_fallbacks;
    unsigned queue_rejections;
    unsigned preview_drops;
} perf_cheevo_snapshot;

void perf_init(void);

void perf_set_hud_active(int active);

void perf_set_capture_active(int active);

int perf_is_capture_active(void);

int perf_is_enabled(void);

int perf_has_samples(void);

uint64_t perf_begin(void);

void perf_end(enum perf_stage stage, uint64_t start);

void perf_record(enum perf_stage stage, double ms);

void perf_frame_complete(int record);

void perf_note_input_change(void);

void perf_note_poll(void);

void perf_note_batch(unsigned frames);

void perf_note_present(void);

void perf_note_netplay(const perf_netplay_snapshot *snapshot);

void perf_note_cheevo(const perf_cheevo_snapshot *snapshot);

unsigned perf_missed_refreshes(void);

void perf_format_hud(char *buf, size_t len, double fps);

int perf_export_trace(const char *path);
