#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/grid.h>

void style_grid_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int content = style_content_bottom(theme, height) - top;
    const int column_width = (int) fmin(width / 4.0, width * 0.2375 * style_spacing());
    const int row_height = (int) fmin(content / 2.0, content / 2.0 * style_spacing());
    const int cell =
        (int) fmin(fmin(column_width, row_height), fmin(column_width * 0.88, row_height * 0.88) * style_size());
    style_set_cells(
        theme, 2, 4, column_width, row_height, cell, (width - column_width * 4) / 2,
        top + (content - row_height * 2) / 2
    );

    style_json_add("{\"mode\":\"items\",\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        style_add_glyph_item(
            index, (index % 4) * column_width + (column_width - cell) / 2.0,
            (index / 4) * row_height + (row_height - cell) / 2.0, cell, cell, theme->grid.cell.radius, 0, NULL
        );
    }
    style_json_add("}}");
}
