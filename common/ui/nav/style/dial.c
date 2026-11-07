#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/dial.h>

#define DIAL_SLOTS 7

void style_dial_apply(struct theme_config *theme, const int width, const int height) {
    const double front = fmin(height * 0.26, width * 0.16) * style_size();
    const double centre_x = width / 2.0;
    const double centre_y = height * 0.95;
    const double radius_x = width * 0.46 * fmin(style_spacing(), 1.08);
    const double radius_y = height * 0.55;
    const int middle = DIAL_SLOTS / 2;

    style_set_cells(theme, 1, DIAL_SLOTS, width / DIAL_SLOTS, (int) (height * 0.8), (int) front, 0, 0);
    theme->grid.carousel_fade = 15;
    theme->grid.animation_time = 360;

    style_json_add(
        "{\"mode\":\"slots\",\"motion\":\"orbit\",\"centre_x\":%d,\"centre_y\":%d,\"radius_x\":%d,\"radius_y\":%d,"
        "\"slots\":[",
        (int) centre_x, (int) centre_y, (int) radius_x, (int) radius_y
    );
    for (int index = 0; index < DIAL_SLOTS; index++) {
        const int offset = index - middle;
        const double angle = offset * M_PI / (DIAL_SLOTS - 1);
        const int size = (int) (front * (0.45 + 0.55 * cos(angle)));
        style_add_slot(
            index, &(style_slot) {centre_x + radius_x * sin(angle), centre_y - radius_y * cos(angle), size, size,
                                  (int) (size * 0.30), 0, 0, 0, abs(offset) == middle ? 0 : 255}
        );
    }
    style_json_add("]}");
}
