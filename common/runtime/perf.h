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

int fe_perf_export_trace(const char *path);

void fe_perf_flush(void);
