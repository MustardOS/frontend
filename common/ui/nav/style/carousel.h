#pragma once

#include <common/display/theme.h>

void style_carousel_cells(
    struct theme_config *theme, int width, int height, int *columns, int *column_width, int *cell
);

void style_carousel_apply(struct theme_config *theme, int width, int height);
