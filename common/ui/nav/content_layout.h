#pragma once

#include <common/config/config.h>
#include <common/display/theme.h>
#include <json/json.h>

#define CONTENT_LAYOUT_GLOBAL INFO_LAY_PATH "/global.json"

enum {
    content_field_style,
    content_field_columns,
    content_field_rows,
    content_field_size,
    content_field_names,
    content_field_strip_columns,
    content_field_strip_size,
    content_field_side,
    content_field_offset_x,
    content_field_offset_y,
    content_field_count_items,
    content_field_spacing,
    content_field_counter_align,
    content_field_counter_x,
    content_field_counter_y,
    content_field_count
};

typedef struct {
    const char *key;
    size_t offset;
    int min;
    int max;
    int step;
    int fallback;
} content_layout_field;

extern const content_layout_field content_layout_fields[content_field_count];

extern const char *const content_layout_screens[content_layout_count];

int16_t *content_layout_value(content_layout_config *layout, int field);

void content_layout_reset(content_layout_config *layout);

void content_layout_load(struct mux_config *config);

int content_layout_save(const struct mux_config *config);

const content_layout_config *content_layout_current(const struct mux_config *config);

content_layout_config *content_layout_folder_config(void);

void content_layout_folder_use(int use);

void content_layout_read(struct json object, content_layout_config *layout);

size_t
content_layout_write(char *out, size_t size, const content_layout_config *layout, const content_layout_config *base);

int content_layout_store(const char *path, const char *document, int empty);

int content_layout_grid_forced(const struct mux_config *config);

int content_layout_mixed(const struct mux_config *config);

int content_layout_side(const struct mux_config *config);

int content_layout_name_bar_highlight(void);

int16_t content_layout_item_count(const struct mux_config *config);

int16_t content_layout_item_spacing(const struct mux_config *config);

void content_layout_apply(struct theme_config *theme, const struct mux_config *config, int width, int height);
