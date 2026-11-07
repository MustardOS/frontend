#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/clock.h>

void style_clock_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const int diameter = (int) (fmin(area * 0.2, width * 0.13) * style_size());
    const double radius = ((area - diameter) / 2.0 - area * 0.02) * fmin(style_spacing(), 1.0);
    const double reach = fmin(radius * 1.3, (width - diameter) / 2.0 - width * 0.03);
    const double middle_y = top + area / 2.0;

    style_set_cells(theme, 2, 4, diameter, diameter, diameter, 0, 0);
    theme->grid.cell.radius = (int16_t) (diameter / 2);
    style_place_label(theme, width / 2.0, middle_y, reach * 2 - diameter * 1.15, width, height);

    double centre_x[STYLE_ITEM_COUNT], centre_y[STYLE_ITEM_COUNT];
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const double angle = index * 2 * M_PI / STYLE_ITEM_COUNT;
        centre_x[index] = width / 2.0 + reach * sin(angle);
        centre_y[index] = middle_y - radius * cos(angle);
    }
    style_add_circles(centre_x, centre_y, diameter, style_swell(115));
}
