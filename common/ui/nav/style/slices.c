#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tweak.h>
#include <common/ui/nav/style/slices.h>

#define SLICES_SLOTS 9

void style_slices_apply(struct theme_config *theme, const int width, const int height) {
    const double slice_h = height;
    const int lean = (int) lround(slice_h * tan(style_number_value(number_slant) * M_PI / 180.0));
    const int slant = abs(lean);
    const double gap = style_number_value(number_gap);
    const double open_face = width * style_number_value(number_focus) / 100.0;
    const double narrow_face = ((width - open_face) / 2 + slant) / 3 - gap;
    const double open_w = open_face + slant;
    const double narrow_w = narrow_face + slant;
    const double centre_y = height / 2.0;
    const int middle = SLICES_SLOTS / 2;

    style_set_cells(theme, 1, SLICES_SLOTS, width / SLICES_SLOTS, (int) slice_h, (int) narrow_w, 0, 0);
    theme->grid.cell.radius = 0;
    theme->grid.cell_default.text_alpha = 0;
    theme->grid.cell_focus.text_alpha = 0;
    style_place_label(theme, width / 2.0, height * 0.78, open_w, width, height);

    style_json_add(
        "{\"mode\":\"slots\",\"art\":\"cover\",\"slant\":%d,\"text\":\"%s\",\"text_x\":%d,\"text_y\":%d,"
        "\"text_angle\":%d,\"text_bg\":%d,\"slots\":[",
        lean, style_slice_text() ? "across" : "none", style_number_value(number_textx),
        style_number_value(number_texty), style_number_value(number_textangle),
        style_number_value(number_textbg) * 255 / 100
    );
    for (int index = 0; index < SLICES_SLOTS; index++) {
        const int offset = index - middle;
        const int distance = abs(offset);
        const int side = offset < 0 ? -1 : 1;
        const double reach = open_face / 2 + gap + (distance - 1) * (narrow_face + gap) + narrow_face / 2;
        const double centre_x = width / 2.0 + (offset ? side * reach : 0);
        const int opacity = distance == middle ? 0 : 255 - distance * 20;
        const style_slot slot = {
            centre_x,
            centre_y,
            (int) (offset ? narrow_w : open_w),
            (int) slice_h,
            0,
            0,
            0,
            0,
            opacity,
            (int) (offset ? narrow_face * 0.6 : open_face * 0.3),
            style_slice_text() ? NULL : "none"
        };
        style_add_slot(index, &slot);
    }
    style_json_add("]}");
}
