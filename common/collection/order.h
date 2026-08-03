#pragma once

#include "common.h"

typedef enum {
    order_natural = 0,
    order_play_time,
    order_times_played,
    order_last_played,
    order_recently_added,
    order_title_length,
    order_file_size,
    order_feeling_lucky,
    order_count
} order_method;

#define ORDER_MAX_VARIANTS 6

extern order_method order_active;
extern int order_variants[order_count];

int order_is_default(void);

extern int order_scope_directory;

const char *order_method_name(order_method method);

const char *order_method_glyph(order_method method);

int order_variant_count(order_method method);

const char *order_variant_name(order_method method, int variant);

void order_load(const char *dir);

void order_save(const char *dir, int directory_scope);

void order_reset_defaults(void);

void order_prepare(content_item *content_items, size_t count, const char *base_dir);

void order_apply(content_item *content_items, size_t count);
