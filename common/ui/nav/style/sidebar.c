#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/sidebar.h>

#define SIDEBAR_SLOTS 7

void style_sidebar_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const double row = area / (double) SIDEBAR_SLOTS;
    const int small = (int) (row * 0.8);
    const int big = (int) fmin(area * 0.5, width * 0.4);
    const double centre_y = top + area / 2.0;
    const double big_x = width * 0.62;
    const double big_y = centre_y - area * 0.08;
    const int middle = SIDEBAR_SLOTS / 2;

    style_set_cells(theme, SIDEBAR_SLOTS, 1, small, (int) row, small, 0, 0);
    style_place_label(theme, big_x, big_y + big / 2.0 + area * 0.1, width * 0.55, width, height);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < SIDEBAR_SLOTS; index++) {
        const int offset = index - middle;
        const int size = offset ? small : big;
        style_add_slot(
            index, &(style_slot) {offset ? width * 0.13 : big_x, offset ? centre_y + offset * row : big_y, size, size,
                                  (int) (size * 0.25), 0, 0, 0, 255}
        );
    }
    style_json_add("]}");
}
