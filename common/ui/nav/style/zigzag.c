#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/zigzag.h>

void style_zigzag_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double gap = width * 0.02 * style_spacing() * style_spacing();
    const double fit_w = (width * 0.92 - gap * 3.5) / 4.5;
    const double fit_h = (area * 0.9 - gap) / 2;
    const double cell = fmin(fit_w, fit_h) * fmin(style_size(), 1.0);
    const double step_x = (cell + gap) / 2;
    const double step_y = cell + gap;
    const double left = (width - step_x * (STYLE_ITEM_COUNT - 1) - cell) / 2;
    const double upper = top + (area - step_y - cell) / 2;

    style_set_cells(theme, 1, STYLE_ITEM_COUNT, (int) step_x, (int) step_y, (int) cell, 0, 0);

    style_json_add("{\"mode\":\"items\",\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        style_add_glyph_item(
            index, left + index * step_x, upper + (index % 2) * step_y, (int) cell, (int) cell, theme->grid.cell.radius,
            0, NULL
        );
    }
    style_json_add("}}");
}
