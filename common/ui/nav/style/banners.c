#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/banners.h>

#define BANNER_SLOTS 7

void style_banners_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const double row = area / 5.5;
    const double banner_h = row * 0.78;
    const double banner_w = width * 0.46;
    const double centre_y = top + area / 2.0;
    const int middle = BANNER_SLOTS / 2;

    style_set_cells(theme, BANNER_SLOTS, 1, (int) banner_w, (int) row, (int) banner_h, 0, 0);
    style_show_cell_text(theme);
    style_hide_name_bar(theme);

    style_json_add("{\"mode\":\"slots\",\"motion\":\"swing\",\"slots\":[");
    for (int index = 0; index < BANNER_SLOTS; index++) {
        const int offset = index - middle;
        const double grow = offset ? 1.0 : 1.12;
        const int w = (int) (banner_w * grow);
        const int h = (int) (banner_h * grow);
        const double left = offset ? width * 0.01 : width * 0.05;
        style_add_slot(
            index, &(style_slot) {left + w / 2.0, centre_y + offset * row, w, h, (int) (h * 0.2), 0, 0, -3,
                                  abs(offset) == middle ? 0 : 255, (int) (h * 0.6), "beside"}
        );
    }
    style_json_add("]}");
}
