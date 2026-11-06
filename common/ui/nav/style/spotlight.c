#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/spotlight.h>

#define SPOTLIGHT_SLOTS 5

void style_spotlight_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double front = fmin(area * 0.62, width * 0.42);
    const double centre_y = top + area / 2.0;
    const double step = front * 0.75 + width * 0.04;
    const int middle = SPOTLIGHT_SLOTS / 2;

    style_set_cells(theme, 1, SPOTLIGHT_SLOTS, width / SPOTLIGHT_SLOTS, (int) front, (int) front, 0, 0);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < SPOTLIGHT_SLOTS; index++) {
        const int offset = index - middle;
        const int distance = abs(offset);
        const int size = (int) (distance ? front * 0.5 : front);
        const double centre_x = width / 2.0 + (distance > 1 ? offset * width * 0.62 : offset * step);
        style_add_slot(
            index, &(style_slot) {centre_x, centre_y, size, size, (int) (size * 0.2), 0, 0, 0,
                                  distance == 0 ? 255 : distance == 1 ? 150 : 0, (int) (size * 0.55)}
        );
    }
    style_json_add("]}");
}
