#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>

const char *get_random_hex(void);

int clamp_range(int value, int min, int max);

int pct_to_int(int pct, int min, int max);

int int_to_pct(int num, int min, int max);

void set_setting_value(const char *script_name, int value, int offset);

int get_index_on_delete(int current_index, int post_delete_count);

int16_t validate_int16(int value, const char *field);

static inline void *mux_malloc(size_t n) {
    void *p = malloc(n);
    if (!p && n) {
        fprintf(stderr, "OOM (%zu bytes)\n", n);
        abort();
    }
    return p;
}

static inline char *mux_strdup(const char *s) {
    char *p = strdup(s);
    if (!p) {
        fprintf(stderr, "OOM\n");
        abort();
    }
    return p;
}

static inline int safe_atoi(const char *s, int fallback) {
    if (!s || !*s) return fallback;
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) return fallback;
    return (int) v;
}
