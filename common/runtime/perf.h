#pragma once

#include <stddef.h>
#include <stdint.h>

enum fe_perf_stage {
    fe_perf_stage_loop = 0,
    fe_perf_stage_input,
    fe_perf_stage_nav,
    fe_perf_stage_list,
    fe_perf_stage_catalogue,
    fe_perf_stage_image,
    fe_perf_stage_glyph,
    fe_perf_stage_font,
    fe_perf_stage_lv_task,
    fe_perf_stage_render,
    fe_perf_stage_idle,
    fe_perf_stage_saver_update,
    fe_perf_stage_saver_render,
    fe_perf_stage_saver_present,
    fe_perf_stage_saver_scan,
    fe_perf_stage_saver_decode,
    fe_perf_stage_count
};

void fe_perf_init(void);

void fe_perf_set_capture_active(int active);

int fe_perf_is_capture_active(void);

int fe_perf_is_enabled(void);

uint64_t fe_perf_begin(void);

void fe_perf_end(enum fe_perf_stage stage, uint64_t start);

void fe_perf_record(enum fe_perf_stage stage, double ms);

void fe_perf_loop_complete(void);

void fe_perf_set_saver(const char *name);

void fe_perf_note_saver_draw_calls(unsigned count);

void fe_perf_note_saver_deadline_miss(unsigned count);

int fe_perf_export_trace(const char *path);

int fe_perf_export_support(const char *path);

void fe_perf_flush(void);
