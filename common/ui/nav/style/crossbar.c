#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/crossbar.h>

#define CROSSBAR_SLOTS 9

void style_crossbar_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double size = fmin(area * 0.24, width * 0.15) * style_size();
    const double step = size * 1.3 * style_spacing();
    const double focus_x = width * (style_position() ? 0.5 : 0.25);
    const double focus_y = top + area * 0.32;
    const int middle = CROSSBAR_SLOTS / 2;

    style_set_cells(theme, 1, CROSSBAR_SLOTS, width / CROSSBAR_SLOTS, (int) size, (int) size, 0, 0);
    style_place_label(theme, focus_x, focus_y + size * 0.95, width * 0.4, width, height);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < CROSSBAR_SLOTS; index++) {
        const int offset = index - middle;
        const int cell = (int) (offset ? size * 0.75 : size * 1.15);
        const int opacity = offset < -1 ? 0 : offset == -1 ? 110 : 255;
        style_add_slot(
            index, &(style_slot) {focus_x + offset * step, focus_y, cell, cell, (int) (cell * 0.22), 0, 0, 0, opacity,
                                  (int) (cell * 0.6)}
        );
    }
    style_json_add("]}");
}
