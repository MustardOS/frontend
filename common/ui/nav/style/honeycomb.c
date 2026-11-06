#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/honeycomb.h>

void style_honeycomb_apply(struct theme_config *theme, const int width, const int height) {
    static const int row_of[STYLE_ITEM_COUNT] = {0, 0, 0, 1, 1, 2, 2, 2};
    static const int row_start[] = {0, 3, 5};
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const int diameter = (int) fmin(area * 0.30, width * 0.2);
    const double step_x = diameter * 1.08;
    const double step_y = step_x * 0.87;
    const double middle_y = top + area / 2.0;

    style_set_cells(theme, 2, 4, diameter, diameter, diameter, 0, 0);
    theme->grid.cell.radius = (int16_t) (diameter / 2);

    double centre_x[STYLE_ITEM_COUNT], centre_y[STYLE_ITEM_COUNT];
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int row = row_of[index];
        const int count = row == 1 ? 2 : 3;
        centre_x[index] = width / 2.0 + (index - row_start[row] - (count - 1) / 2.0) * step_x;
        centre_y[index] = middle_y + (row - 1) * step_y;
    }
    style_add_circles(centre_x, centre_y, diameter, 112);
}
