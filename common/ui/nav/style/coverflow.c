#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/coverflow.h>
#include <common/ui/nav/style/carousel.h>

void style_coverflow_apply(struct theme_config *theme, const int width, const int height) {
    static const double sizes[] = {1.25, 0.95, 0.80};
    const double turn = 50.0 * style_tilt();
    const double squash = cos(turn * M_PI / 180.0);

    int columns, column_width, cell;
    style_carousel_cells(theme, width, height, &columns, &column_width, &cell);

    const double gap = cell * 0.05 * style_spacing() + cell * (style_spacing() - 1);
    const int middle = columns / 2;

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"reflect\":70,\"slots\":[");
    for (int index = 0; index < columns; index++) {
        const int distance = abs(index - middle);
        const int size = (int) (cell * sizes[distance]);
        double offset = 0.0;
        for (int step = 1; step <= distance; step++) {
            const double previous = cell * sizes[step - 1] * (step == 1 ? 1.0 : squash);
            offset += previous / 2 + cell * sizes[step] * squash / 2 + gap;
        }
        const int side = index > middle ? 1 : -1;
        style_add_slot(
            index, &(style_slot) {columns * column_width / 2.0 + side * offset, (int) (cell * 1.25 / 2), size, size,
                                  (int) (size * 0.22), 0, distance ? (int) (-side * turn) : 0, 0, 255}
        );
    }
    style_json_add("]}");
}
