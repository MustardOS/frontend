#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/stack.h>

#define STACK_SLOTS 7

void style_stack_apply(struct theme_config *theme, const int width, const int height) {
    static const double anchors[] = {0.32, 0.5, 0.68};
    const double front = fmin(height * 0.40, width * 0.30) * style_size();
    const double top = height * 0.52 - front / 2;
    const double anchor = width * anchors[style_position()];
    const int pile = style_pile();
    const int side = pile == 2 ? -1 : 1;
    const int middle = STACK_SLOTS / 2;

    style_set_cells(theme, 1, STACK_SLOTS, width / STACK_SLOTS, (int) (height * 0.8), (int) front, 0, 0);
    style_solid_cells(theme);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < STACK_SLOTS; index++) {
        const int depth = middle - index;
        const int size = depth > 0 ? (int) (front * (1 - 0.1 * depth)) : (int) front;
        const double shift = pile == 1 ? -front * 0.13 * depth : pile == 2 ? front * 0.13 * depth : 0;
        const double centre_x = depth < 0 ? anchor + side * front * 1.4 : anchor + (depth > 0 ? shift : 0);
        const double centre_y = (depth > 0 && pile == 0 ? top - front * 0.13 * depth : top) + size / 2.0;
        style_add_slot(
            index, &(style_slot) {centre_x, centre_y, size, size, (int) (size * 0.12), 0, depth < 0 ? -40 * side : 0, 0,
                                  depth < 0 ? 0 : 255}
        );
    }
    style_json_add("]}");
}
