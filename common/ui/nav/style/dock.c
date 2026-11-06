#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/dock.h>

#define DOCK_SLOTS 7

void style_dock_apply(struct theme_config *theme, const int width, const int height) {
    static const double sizes[] = {1.0, 0.7, 0.55, 0.45};
    const double front = fmin(height * 0.22, width * 0.19);
    const double gap = front * 0.1;
    const double base = height - theme->footer.height - height * 0.04;
    const int middle = DOCK_SLOTS / 2;

    style_set_cells(theme, 1, DOCK_SLOTS, width / DOCK_SLOTS, (int) front, (int) front, 0, 0);
    theme->grid.carousel_fade = 10;
    style_place_label(theme, width / 2.0, (theme->header.height + base - front) / 2.0, width * 0.8, width, height);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < DOCK_SLOTS; index++) {
        const int distance = abs(index - middle);
        const int size = (int) (front * sizes[distance]);
        double offset = 0.0;
        for (int step = 1; step <= distance; step++) {
            offset += front * (sizes[step - 1] + sizes[step]) / 2 + gap;
        }
        style_add_slot(
            index, &(style_slot) {width / 2.0 + (index < middle ? -offset : offset), base - size / 2.0, size, size,
                                  (int) (size * 0.25), 0, 0, 0, 255}
        );
    }
    style_json_add("]}");
}
