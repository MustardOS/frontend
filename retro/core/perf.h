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
    perf_stage_count
};

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

unsigned perf_missed_refreshes(void);

void perf_format_hud(char *buf, size_t len, double fps);

int perf_export_trace(const char *path);
