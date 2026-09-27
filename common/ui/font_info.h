#pragma once

#include <stdint.h>
#include <common/base/options.h>

typedef struct {
    char family[MAX_BUFFER_SIZE];
    char style[MAX_BUFFER_SIZE];
    char display[MAX_BUFFER_SIZE];
    unsigned int face_count;
} font_info_t;

typedef struct {
    uint32_t tag;
    float minimum;
    float default_value;
    float maximum;
    char name[MAX_BUFFER_SIZE];
} font_axis_info_t;

int font_info_read(const char *path, unsigned int face_index, font_info_t *info);

int font_info_axis(const char *path, unsigned int face_index, uint32_t tag, font_axis_info_t *axis);

int font_info_fixed_sizes(const char *path, unsigned int face_index, int *sizes, size_t capacity);
