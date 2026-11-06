#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/carousel.h>

void style_carousel_cells(
    struct theme_config *theme, const int width, const int height, int *columns, int *column_width, int *cell
) {
    *columns = width * 2 >= height * 3 ? 5 : 3;
    *column_width = (int) (width * 0.94 / *columns);
    *cell = (int) fmin(*column_width * 0.84, height * 0.40);
    style_set_cells(
        theme, 1, *columns, *column_width, (int) (*cell * 1.15), *cell, (width - *columns * *column_width) / 2,
        (int) ((height - *cell) / 2.0 - height * 0.05)
    );
    theme->grid.carousel_fade = 30;
    theme->grid.layout_slot_gap = (int16_t) lround(36.0 * height / 1080.0);
}

void style_carousel_apply(struct theme_config *theme, const int width, const int height) {
    int columns, column_width, cell;
    style_carousel_cells(theme, width, height, &columns, &column_width, &cell);
}
