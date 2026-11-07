#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/orbit.h>

#define ORBIT_SLOTS 9

void style_orbit_apply(struct theme_config *theme, const int width, const int height) {
    const double front = fmin(height * 0.28, width * 0.16) * style_size();
    const double centre_x = width / 2.0;
    const double centre_y = height * 0.43;
    const double radius_x = fmin(width * 0.40 * style_spacing(), width * 0.46);
    const double radius_y = fmin(height * 0.22 * style_spacing(), height * 0.27);
    const int middle = ORBIT_SLOTS / 2;

    style_set_cells(theme, 1, ORBIT_SLOTS, width / ORBIT_SLOTS, (int) (height * 0.8), (int) front, 0, 0);
    theme->grid.carousel_fade = 20;
    theme->grid.animation_time = 420;

    style_json_add("{\"mode\":\"slots\",\"motion\":\"orbit\",\"slots\":[");
    for (int index = 0; index < ORBIT_SLOTS; index++) {
        const int offset = index - middle;
        const double angle = offset * 2 * M_PI / (ORBIT_SLOTS - 1);
        const int size = (int) (front * (0.40 + 0.60 * (1 + cos(angle)) / 2));
        const style_slot slot = {
            centre_x + radius_x * sin(angle),
            centre_y + radius_y * cos(angle),
            size,
            size,
            (int) (size * 0.30),
            0,
            offset ? (int) lround(-35 * style_tilt() * sin(angle)) : 0,
            0,
            255
        };
        style_add_slot(index, &slot);
    }
    style_json_add("]}");
}
