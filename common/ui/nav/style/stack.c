#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/stack.h>

#define STACK_SLOTS 7

void style_stack_apply(struct theme_config *theme, const int width, const int height) {
    const double front = fmin(height * 0.40, width * 0.30);
    const double top = height * 0.52 - front / 2;
    const int middle = STACK_SLOTS / 2;

    style_set_cells(theme, 1, STACK_SLOTS, width / STACK_SLOTS, (int) (height * 0.8), (int) front, 0, 0);
    style_solid_cells(theme);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < STACK_SLOTS; index++) {
        const int depth = middle - index;
        const int size = depth > 0 ? (int) (front * (1 - 0.1 * depth)) : (int) front;
        const double centre_x = depth < 0 ? width / 2.0 + front * 1.4 : width / 2.0;
        const double centre_y = (depth > 0 ? top - front * 0.13 * depth : top) + size / 2.0;
        style_add_slot(
            index, &(style_slot) {centre_x, centre_y, size, size, (int) (size * 0.12), 0, depth < 0 ? -40 : 0, 0,
                                  depth < 0 ? 0 : 255}
        );
    }
    style_json_add("]}");
}
