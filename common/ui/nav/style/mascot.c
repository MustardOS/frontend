#include <math.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tweak.h>
#include <common/ui/nav/style/mascot.h>
#include <common/ui/nav/mascot.h>

void style_mascot_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int bottom = style_content_bottom(theme, height);
    const int area = bottom - top;
    const double margin = width * 0.03 * style_spacing();
    const int mascot = area / 5 > 24 ? area / 5 : 24;
    const double top_w = (width - margin * 5) / 4 * fmin(style_size(), 1.1);
    const int sprite = mascot_sprite_height();
    const int scale = style_number_value(number_mascotscale);
    const double drawn = (sprite ? sprite : mascot) * scale / 100.0;
    const double perched = drawn * 0.75 + (sprite ? drawn * style_bounce_height() / 100.0 : 0.0);
    const double room = (area - perched - margin * 2) / 1.5;
    const double top_h = fmin(fmin(area * 0.44, top_w * 1.2) * fmin(style_size(), 1.1), fmax(room, area * 0.2));
    const double low_w = top_w * 0.8;
    const double low_h = top_h * 0.5;
    const double left = (width - (top_w * 4 + margin * 3)) / 2;
    const double top_y = top + perched;
    const double low_y = bottom - margin - low_h;

    style_set_cells(theme, 2, 4, width / 4, area / 2, (int) top_w, 0, 0);

    static const char *const sides[] = {"between", "above", "below"};
    style_json_add(
        "{\"mode\":\"items\",\"art\":\"native\",\"mascot\":%d,\"mascot_scale\":%d,\"mascot_side\":\"%s\","
        "\"items\":{",
        mascot, scale, sides[style_perch()]
    );
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        const int column = index % 4;
        const int lower = index >= 4;
        const double w = lower ? low_w : top_w;
        const double h = lower ? low_h : top_h;
        const double x = left + column * (top_w + margin) + (top_w - w) / 2;
        style_add_glyph_item(
            index, x, lower ? low_y : top_y, (int) w, (int) h, theme->grid.cell.radius, (int) (fmin(w, h) * 0.42), NULL
        );
    }
    style_json_add("}}");
}
