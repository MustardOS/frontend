#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/grid.h>

void style_grid_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int content = style_content_bottom(theme, height) - top;
    const int column_width = width * 95 / 400;
    const int row_height = content / 2;
    const int cell = (int) fmin(column_width * 0.88, row_height * 0.88);
    style_set_cells(
        theme, 2, 4, column_width, row_height, cell, (width - column_width * 4) / 2,
        top + (content - row_height * 2) / 2
    );
}
