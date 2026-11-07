#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tabbar.h>

#define TABBAR_SLOTS 7

void style_tabbar_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = style_content_bottom(theme, height) - top;
    const double tab = fmin(width / 6.0 * 0.7, area * 0.16) * fmin(style_size(), 1.1);
    const double tab_y = top + tab * 0.75;
    const int big = (int) (fmin(area * 0.5, width * 0.36) * style_size());
    const double big_y = tab_y + tab / 2 + (area - tab * 1.25) / 2;
    const int middle = TABBAR_SLOTS / 2;

    style_set_cells(theme, 1, TABBAR_SLOTS, width / TABBAR_SLOTS, (int) tab, (int) tab, 0, 0);
    style_place_label(theme, width / 2.0, big_y + big / 2.0 + area * 0.07, width * 0.5, width, height);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < TABBAR_SLOTS; index++) {
        const int offset = index - middle;
        const int place = offset < 0 ? offset + middle : offset + middle - 1;
        const int size = offset ? (int) tab : big;
        style_add_slot(
            index,
            &(style_slot) {offset ? width * (place + 0.5) / (TABBAR_SLOTS - 1) : width / 2.0, offset ? tab_y : big_y,
                           size, size, (int) (size * 0.22), 0, 0, 0, 255, (int) (size * 0.6)}
        );
    }
    style_json_add("]}");
}
