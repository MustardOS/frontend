#pragma once

#define HISTORY_DEPTH_MAX 10

enum history_source {
    history_source_quick = 0,
    history_source_auto,
    history_source_timeline,
    history_source_standard,
    history_source_count
};

void history_set_directory(const char *state_dir);

void history_push(enum history_source source);

int history_count(void);

int history_describe(int index, enum history_source *source, long long *created);

const char *history_thumbnail(int index);

int history_restore(int index);

int history_effective_depth(void);

void history_clear(void);

void history_shutdown(void);
