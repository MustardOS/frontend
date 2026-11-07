#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/honeycomb.h>

void style_honeycomb_apply(struct theme_config *theme, const int width, const int height) {
    static const int row_of[STYLE_ITEM_COUNT] = {0, 0, 0, 1, 1, 2, 2, 2};
    static const int row_start[] = {0, 3, 5};
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double gap = area * 0.02 * style_spacing() * style_spacing();
    const double hex_h = fmin((area - gap * 2) / 2.5, (width * 0.6 - gap * 2) / (3 * 0.866)) * fmin(style_size(), 1.0);
    const double hex_w = hex_h * 0.866;
    const double step_x = hex_w + gap;
    const double step_y = hex_h * 0.75 + gap * 0.866;
    const double middle_y = top + area / 2.0;

    style_set_cells(theme, 2, 4, (int) hex_w, (int) hex_h, (int) hex_w, 0, 0);
    theme->grid.cell.radius = 0;

    style_json_add("{\"mode\":\"items\",\"shape\":\"hexagon\",\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int row = row_of[index];
        const int count = row == 1 ? 2 : 3;
        const double centre_x = width / 2.0 + (index - row_start[row] - (count - 1) / 2.0) * step_x;
        const double centre_y = middle_y + (row - 1) * step_y;
        style_add_glyph_item(
            index, centre_x - hex_w / 2, centre_y - hex_h / 2, (int) hex_w, (int) hex_h, 0, (int) (hex_w * 0.5), NULL
        );
    }
    style_json_add("}}");
}
