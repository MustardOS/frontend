#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tiles.h>

void style_tiles_apply(struct theme_config *theme, const int width, const int height) {
    static const int tiles[STYLE_ITEM_COUNT][4] = {{0, 0, 2, 2}, {2, 0, 2, 1}, {2, 1, 1, 1}, {3, 1, 1, 1},
                                                   {0, 2, 1, 1}, {1, 2, 1, 1}, {2, 2, 1, 1}, {3, 2, 1, 1}};
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double gap = width * 0.015 * style_spacing() * style_spacing();
    const double left = width * 0.06;
    const double unit_w = (width - left * 2 - gap * 3) / 4;
    const double unit_h = (area - gap * 4) / 3;
    const int radius = (int) (fmin(unit_w, unit_h) * 0.15);

    style_set_cells(theme, 2, 4, (int) unit_w, (int) unit_h, (int) fmin(unit_w, unit_h), 0, 0);
    theme->grid.cell.radius = (int16_t) radius;

    style_json_add("{\"mode\":\"items\",\"focus_zoom\":104,\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int *tile = tiles[index];
        style_add_item(
            index, left + tile[0] * (unit_w + gap), top + gap + tile[1] * (unit_h + gap),
            (int) (tile[2] * unit_w + (tile[2] - 1) * gap), (int) (tile[3] * unit_h + (tile[3] - 1) * gap), radius
        );
    }
    style_json_add("}}");
}
