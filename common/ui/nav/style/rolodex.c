#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/rolodex.h>

#define ROLODEX_SLOTS 7

void style_rolodex_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const double front = fmin(area * 0.24, width * 0.18) * style_size();
    const double centre_x = width * 0.30;
    const double centre_y = top + area / 2.0;
    const double radius = area * 0.47;
    const int middle = ROLODEX_SLOTS / 2;

    style_set_cells(theme, ROLODEX_SLOTS, 1, (int) front, area / ROLODEX_SLOTS, (int) front, 0, 0);
    theme->grid.carousel_fade = 15;
    style_place_label(theme, width * 0.68, centre_y, width * 0.5, width, height);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < ROLODEX_SLOTS; index++) {
        const int offset = index - middle;
        const double angle = offset * M_PI / (ROLODEX_SLOTS - 1);
        const int size = (int) (front * (0.5 + 0.5 * cos(angle)));
        style_add_slot(
            index, &(style_slot) {centre_x, centre_y + radius * sin(angle), size, size, (int) (size * 0.25),
                                  (int) lround(-35 * style_tilt() * sin(angle)), 0, 0, abs(offset) == middle ? 0 : 255}
        );
    }
    style_json_add("]}");
}
