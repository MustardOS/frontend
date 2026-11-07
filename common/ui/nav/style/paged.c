#include <math.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/paged.h>

#define PAGED_COLUMNS 4

void style_paged_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const double column = width / (PAGED_COLUMNS + 0.4) * fmin(style_spacing(), 1.0 + 0.4 / PAGED_COLUMNS);
    const double cell = fmin(column * 0.82, area * 0.42) * style_size();
    const double left = (width - column * PAGED_COLUMNS) / 2 + (column - cell) / 2;
    const double cell_y = top + (area - cell) / 2 - area * 0.06;

    style_set_cells(theme, 2, PAGED_COLUMNS, (int) column, (int) cell, (int) cell, 0, 0);
    theme->grid.cell.radius = (int16_t) (cell * 0.18);

    style_json_add("{\"mode\":\"items\",\"focus_zoom\":%d,\"items\":{", style_swell(106));
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int page = index / PAGED_COLUMNS;
        style_add_glyph_item(
            index, page * width + left + (index % PAGED_COLUMNS) * column, cell_y, (int) cell, (int) cell,
            (int) (cell * 0.18), (int) (cell * 0.5), NULL
        );
    }
    style_json_add("}}");
}
