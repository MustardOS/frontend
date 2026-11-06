#include <math.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/bubbles.h>

#define BUBBLE_ASPECT 1.88

void style_bubbles_apply(struct theme_config *theme, const int width, const int height) {
    static const double bubbles[STYLE_ITEM_COUNT][3] = {
        {0.30, 0.42, 0.46}, {0.55, 0.30, 0.34}, {0.75, 0.50, 0.30}, {0.52, 0.72, 0.30},
        {0.12, 0.72, 0.24}, {0.88, 0.22, 0.22}, {0.12, 0.22, 0.20}, {0.90, 0.80, 0.20},
    };
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double unit = fmin(area, width / BUBBLE_ASPECT);
    const double left = (width - unit * BUBBLE_ASPECT) / 2;
    const double upper = top + (area - unit) / 2;

    style_set_cells(theme, 2, 4, (int) (unit * 0.46), (int) (unit * 0.46), (int) (unit * 0.46), 0, 0);

    style_json_add("{\"mode\":\"items\",\"focus_zoom\":118,\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int diameter = (int) (unit * bubbles[index][2]);
        style_add_item(
            index, left + unit * BUBBLE_ASPECT * bubbles[index][0] - diameter / 2.0,
            upper + unit * bubbles[index][1] - diameter / 2.0, diameter, diameter, diameter / 2
        );
    }
    style_json_add("}}");
}
