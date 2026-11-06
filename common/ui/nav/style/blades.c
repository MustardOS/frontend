#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/blades.h>

#define BLADES_SLOTS 9

void style_blades_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double blade_h = area * 0.82;
    const double blade_w = width * 0.06;
    const double open_w = width * 0.4;
    const double open_left = width * 0.14;
    const double step = blade_w + width * 0.012;
    const double centre_y = top + area / 2.0;
    const int middle = BLADES_SLOTS / 2;

    style_set_cells(theme, 1, BLADES_SLOTS, width / BLADES_SLOTS, (int) blade_h, (int) blade_w, 0, 0);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < BLADES_SLOTS; index++) {
        const int offset = index - middle;
        const int distance = abs(offset);
        const double centre_x = offset == 0  ? open_left + open_w / 2
                                : offset > 0 ? open_left + open_w + width * 0.02 + (offset - 0.5) * step
                                             : open_left - width * 0.02 - (distance - 0.5) * step;
        const int w = (int) (offset ? blade_w : open_w);
        style_add_slot(
            index, &(style_slot) {centre_x, centre_y, w, (int) blade_h, (int) (blade_w * 0.3), 0,
                                  offset ? (offset > 0 ? -20 : 20) : 0, 0, distance == middle ? 0 : 255,
                                  (int) (offset ? blade_w * 0.7 : open_w * 0.4)}
        );
    }
    style_json_add("]}");
}
