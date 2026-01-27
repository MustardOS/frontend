#pragma once

typedef struct {
    int enabled;
    float matrix[9];
} colour_filter_matrix_t;

const colour_filter_matrix_t *colour_filter_get(void);

void colour_filter_reset(void);
