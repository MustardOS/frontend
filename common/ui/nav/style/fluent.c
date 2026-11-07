#include <math.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/fluent.h>

void style_fluent_apply(struct theme_config *theme, const int width, const int height) {
    const int blocks = style_large() ? 4 : 3;
    const int tools = STYLE_ITEM_COUNT - blocks;
    const int top = theme->header.height;
    const double gap = width * 0.0125 * style_spacing() * style_spacing();
    const double small = height * 0.14;
    const double tool_y = height - theme->footer.height - gap - small;
    const double block_w = (width - gap * (blocks + 1)) / blocks;
    const double block_h = tool_y - gap * 2 - top;
    const double split = gap + (block_w + gap) * (blocks - 1) - gap / 2;
    const double paired = (block_w + gap / 2 - small * 2) / 1.5 - 1;
    const double span_left = gap + (block_w + gap) * (blocks - 2);
    const double even = (block_w * 2 + gap - small * tools) / (tools - 1);

    style_set_cells(theme, 2, 4, (int) block_w, (int) block_h, (int) fmin(block_w, block_h), 0, 0);
    style_show_cell_text(theme);
    theme->grid.cell.image_padding_top = (int16_t) (block_h * 0.3);
    theme->grid.cell.text_padding_bottom = (int16_t) (block_h * 0.3);
    theme->grid.cell.text_padding_side = (int16_t) (block_w * 0.06);

    style_hide_name_bar(theme);

    style_json_add("{\"mode\":\"items\",\"focus_zoom\":103,\"items\":{");
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        if (index < blocks) {
            style_add_glyph_item(
                index, gap + index * (block_w + gap), top + gap, (int) block_w, (int) block_h, (int) (block_w * 0.05),
                (int) (block_w * 0.42), NULL
            );
            continue;
        }
        const int tool = index - blocks;
        const double x =
            tools == 4 ? split + (tool - 2) * (small + paired) + paired / 2 : span_left + tool * (small + even);
        style_add_glyph_item(
            index, x, tool_y, (int) small, (int) small, (int) (small * 0.15), (int) (small * 0.5), "none"
        );
    }
    style_json_add("}}");
}
