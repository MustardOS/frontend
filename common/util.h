#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *get_random_hex(void);

int clamp_range(int value, int min, int max);

int pct_to_int(int pct, int min, int max);

int int_to_pct(int num, int min, int max);

void set_setting_value(const char *script_name, int value, int offset);

int get_index_on_delete(int current_index, int post_delete_count);

int16_t validate_int16(int value, const char *field);

void *mux_malloc(size_t size);

char *mux_strdup(const char *text);

int safe_atoi(const char *text, int fallback);
