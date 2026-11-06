#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <common/config/config.h>
#include <common/ui/nav/grid_nav.h>
#include <common/ui/nav/style/shared.h>

static char json_buffer[8192];
static size_t json_used = 0;

static const char *item_names[STYLE_ITEM_COUNT] = {"explore", "collection", "history", "apps",
                                                   "info",    "config",     "reboot",  "shutdown"};

void style_json_reset(void) {
    json_used = 0;
    json_buffer[0] = '\0';
}

const char *style_json(void) {
    return json_used > 0 && json_used < sizeof(json_buffer) ? json_buffer : NULL;
}

void style_json_add(const char *format, ...) {
    if (json_used >= sizeof(json_buffer)) return;

    va_list args;
    va_start(args, format);
    const int written = vsnprintf(json_buffer + json_used, sizeof(json_buffer) - json_used, format, args);
    va_end(args);

    if (written > 0) json_used += (size_t) written;
}

void style_add_slot(const int index, const style_slot *slot) {
    style_json_add(
        "%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d,\"rotate_x\":%d,\"rotate_y\":%d,\"angle\":%d,"
        "\"opacity\":%d,\"glyph\":%d%s%s%s}",
        index ? "," : "", (int) lround(slot->cx - slot->w / 2.0), (int) lround(slot->cy - slot->h / 2.0), slot->w,
        slot->h, slot->radius, slot->rotate_x, slot->rotate_y, slot->angle, slot->opacity, slot->glyph,
        slot->label ? ",\"label\":\"" : "", slot->label ? slot->label : "", slot->label ? "\"" : ""
    );
}

void style_add_glyph_item(
    const int index, const double x, const double y, const int w, const int h, const int radius, const int glyph,
    const char *label
) {
    style_json_add(
        "%s\"%s\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d,\"glyph\":%d%s%s%s}", index ? "," : "",
        item_names[menu_item_order()[index]], (int) lround(x), (int) lround(y), w, h, radius, glyph,
        label ? ",\"label\":\"" : "", label ? label : "", label ? "\"" : ""
    );
}

void style_add_item(const int index, const double x, const double y, const int w, const int h, const int radius) {
    style_add_glyph_item(index, x, y, w, h, radius, 0, NULL);
}

void style_add_circles(const double *centre_x, const double *centre_y, const int diameter, const int focus_zoom) {
    style_json_add("{\"mode\":\"items\",\"focus_zoom\":%d,\"items\":{", focus_zoom);
    for (int index = 0; index < STYLE_ITEM_COUNT; index++) {
        style_add_item(
            index, centre_x[index] - diameter / 2.0, centre_y[index] - diameter / 2.0, diameter, diameter, diameter / 2
        );
    }
    style_json_add("}}");
}

void style_set_cells(
    struct theme_config *theme, const int rows, const int columns, const int column_width, const int row_height,
    const int cell, const int x, const int y
) {
    theme->grid.row_count = (int16_t) rows;
    theme->grid.column_count = (int16_t) columns;
    theme->grid.column_width = (int16_t) column_width;
    theme->grid.row_height = (int16_t) row_height;
    theme->grid.location_x = (int16_t) x;
    theme->grid.location_y = (int16_t) y;
    theme->grid.cell.width = (int16_t) cell;
    theme->grid.cell.height = (int16_t) cell;
    theme->grid.cell.column_align = 1;
    theme->grid.cell.row_align = 1;
    theme->grid.cell.radius = (int16_t) (cell * 3 / 10);
    theme->grid.carousel_fade = 0;
    theme->grid.layout_slot_gap = 0;
    theme->grid.animation_time = 380;
    theme->grid.navigation_type = nav_standard;
}

void style_hide_name_bar(struct theme_config *theme) {
    theme->grid.current_item_label.background_alpha = 0;
    theme->grid.current_item_label.border_alpha = 0;
    theme->grid.current_item_label.text_alpha = 0;
    theme->grid.current_item_label.shadow_width = 0;
}

void style_show_cell_text(struct theme_config *theme) {
    theme->grid.cell_default.text_alpha = 255;
    theme->grid.cell_focus.text_alpha = 255;
}

void style_solid_cells(struct theme_config *theme) {
    theme->grid.cell_default.background_alpha = 255;
    theme->grid.cell_focus.background_alpha = 255;
}

void style_place_label(
    struct theme_config *theme, const double x, const double y, const double label_width, const int width,
    const int height
) {
    theme->grid.current_item_label.alignment = LV_ALIGN_CENTER;
    theme->grid.current_item_label.width = (int16_t) label_width;
    theme->grid.current_item_label.offset_x = (int16_t) (x - width / 2.0);
    theme->grid.current_item_label.offset_y = (int16_t) (y - height / 2.0);
}

int style_content_bottom(const struct theme_config *theme, const int height) {
    if (!config.settings.themeopt.menu_label) return height - theme->footer.height;

    const int label = theme->grid.current_item_label.height + abs(theme->grid.current_item_label.offset_y);
    return height - (label > theme->footer.height ? label : theme->footer.height);
}
