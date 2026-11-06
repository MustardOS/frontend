#include <math.h>
#include <stdlib.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/desktop.h>

#define DESKTOP_ROWS 4

void style_desktop_apply(struct theme_config *theme, const int width, const int height) {
    const int top = theme->header.height;
    const int area = height - top - theme->footer.height;
    const double margin = width * 0.03;
    const double row = (area - margin) / DESKTOP_ROWS;
    const int cell_h = (int) (row * 0.92);
    const int cell_w = (int) (cell_h * 1.9);
    const int icon = (int) (cell_h * 0.42);

    style_set_cells(theme, 2, 4, cell_w, (int) row, cell_h, 0, 0);
    theme->grid.cell_default.background_alpha = 0;
    theme->grid.cell_default.border_alpha = 0;
    theme->grid.cell.shadow_width = 0;
    theme->grid.cell.image_padding_top = (int16_t) (cell_h * 0.04);
    theme->grid.cell.text_padding_bottom = (int16_t) (cell_h * 0.03);
    theme->grid.cell.text_padding_side = (int16_t) (cell_w * 0.04);
    style_show_cell_text(theme);
    style_hide_name_bar(theme);

    style_json_add("{\"mode\":\"items\",\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        style_add_glyph_item(
            index, margin + (index / DESKTOP_ROWS) * (cell_w + margin), top + margin + (index % DESKTOP_ROWS) * row,
            cell_w, cell_h, (int) (cell_h * 0.12), icon, NULL
        );
    }
    style_json_add("}}");
}
