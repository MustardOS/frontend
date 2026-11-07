#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/fan.h>

#define FAN_SLOTS 7
#define FAN_TURN  17

void style_fan_apply(struct theme_config *theme, const int width, const int height) {
    const double card_h = height * 0.34 * style_size();
    const double card_w = card_h * 0.72;
    const double pivot_y = height * 1.6;
    const double radius = pivot_y - height * 0.47;
    const int middle = FAN_SLOTS / 2;

    style_set_cells(theme, 1, FAN_SLOTS, width / FAN_SLOTS, (int) card_h, (int) card_w, 0, 0);
    theme->grid.carousel_fade = 15;

    style_json_add(
        "{\"mode\":\"slots\",\"motion\":\"orbit\",\"centre_x\":%d,\"centre_y\":%d,\"radius_x\":%d,\"radius_y\":%d,"
        "\"slots\":[",
        width / 2, (int) pivot_y, (int) radius, (int) radius
    );
    for (int index = 0; index < FAN_SLOTS; index++) {
        const int offset = index - middle;
        const double turn = FAN_TURN * style_spacing();
        const double angle = offset * turn * M_PI / 180.0;
        const double reach = radius + (offset ? 0 : card_h * 0.2);
        style_add_slot(
            index, &(style_slot) {width / 2.0 + reach * sin(angle), pivot_y - reach * cos(angle), (int) card_w,
                                  (int) card_h, (int) (card_w * 0.1), 0, 0, (int) lround(offset * turn * style_tilt()),
                                  abs(offset) == middle ? 0 : 255}
        );
    }
    style_json_add("]}");
}
