#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/config/config.h>
#include <common/display/theme.h>
#include <common/ui/nav/grid_nav.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tweak.h>

static char json_buffer[8192];
static size_t json_used = 0;

static int layout_values[LAYOUT_OPTION_COUNT];
static int layout_mirror = 0;
static int layout_width = 0;
static int layout_height = 0;
static int slot_middle = 0;
static int slots_written = 0;
static int items_written = 0;

void style_layout_set(const int *values, const int mirror, const int width, const int height) {
    for (int i = 0; i < LAYOUT_OPTION_COUNT; i++)
        layout_values[i] = values[i];
    layout_mirror = mirror;
    layout_width = width;
    layout_height = height;
}

void style_json_root(const char *fields) {
    const size_t extra = strlen(fields);
    if (json_used + extra >= sizeof(json_buffer) || json_buffer[0] != '{') return;

    memmove(json_buffer + 1 + extra, json_buffer + 1, json_used);
    memcpy(json_buffer + 1, fields, extra);
    json_used += extra;
}

static void tweak_box(const style_tweak *tweak, double *cx, double *cy, int *w, int *h) {
    *cx += style_tweak_get(tweak, tweak_dx, 0) * layout_width / 1000.0;
    *cy += style_tweak_get(tweak, tweak_dy, 0) * layout_height / 1000.0;
    *w += style_tweak_get(tweak, tweak_dw, 0) * layout_width / 1000;
    *h += style_tweak_get(tweak, tweak_dh, 0) * layout_height / 1000;
    if (*w < 8) *w = 8;
    if (*h < 8) *h = 8;
}

static const int legacy_numbers[][3] = {
    {85, 100, 115}, {80, 100, 125}, {0, 100, 160}, {0, 100, 200}, {80, 100, 125},
};

static const int legacy_options[] = {layout_size, layout_spacing, layout_tilt, layout_swell, layout_text};

int style_number_default(const int id) {
    if (id < (int) (sizeof(legacy_options) / sizeof(legacy_options[0]))) {
        return legacy_numbers[id][LV_CLAMP(0, layout_values[legacy_options[id]], 2)];
    }
    if (id == number_focus) return SLICE_FOCUS;
    if (id == number_gap) return (int) lround(layout_width * 0.01);
    if (id == number_textbg) return 50;
    if (id == number_textx || id == number_textangle) return 0;
    if (id == number_namebg) return theme.grid.current_item_label.background_alpha * 100 / 255;
    if (id == number_namex || id == number_namey || id == number_nameangle) return 0;
    if (id == number_texty) return layout_height - theme.footer.height - (int) lround(layout_height * 0.06);
    return SLICE_SLANT;
}

int style_number_value(const int id) {
    const style_tweaks *tweaks = style_tweaks_current();
    return tweaks->number_set & (1u << id) ? tweaks->number[id] : style_number_default(id);
}

double style_size(void) {
    return style_number_value(number_size) / 100.0;
}

double style_spacing(void) {
    return style_number_value(number_spacing) / 100.0;
}

double style_tilt(void) {
    return style_number_value(number_tilt) / 100.0;
}

int style_swell(const int base) {
    const int amount = style_number_value(number_swell);
    return amount ? 100 + (base - 100) * amount / 100 : 0;
}

int style_position(void) {
    return layout_values[layout_position];
}

int style_bounce_height(void) {
    const style_tweaks *tweaks = style_tweaks_current();
    return tweaks->bounce_set & 1 ? tweaks->bounce_height : BOUNCE_HEIGHT;
}

int style_bounce_speed(void) {
    const style_tweaks *tweaks = style_tweaks_current();
    return tweaks->bounce_set & 2 ? tweaks->bounce_speed : BOUNCE_SPEED;
}

int style_slice_text(void) {
    return layout_values[layout_slicetext];
}

int style_perch(void) {
    return layout_values[layout_perch];
}

int style_names(void) {
    return layout_values[layout_names];
}

int style_pile(void) {
    return layout_values[layout_pile];
}

int style_large(void) {
    return layout_values[layout_large];
}

static const char *item_names[STYLE_ITEM_COUNT] = {"explore", "collection", "history", "apps",
                                                   "info",    "config",     "reboot",  "shutdown"};

void style_json_reset(void) {
    slots_written = 0;
    items_written = 0;
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

void style_add_slot(const int index, const style_slot *source) {
    const style_tweaks *tweaks = style_tweaks_current();
    const style_tweak *tweak = &tweaks->slots[index < TWEAK_SLOTS ? index : TWEAK_SLOTS - 1];
    if (index >= slots_written) slots_written = index + 1;
    style_slot adjusted = *source;

    const int distance = abs(index - slot_middle);
    const int side = index < slot_middle ? -1 : 1;
    if (distance) {
        const double grow = 1.0 + tweaks->cascade[cascade_size] * distance / 100.0;
        adjusted.w = (int) (adjusted.w * (grow > 0.1 ? grow : 0.1));
        adjusted.h = (int) (adjusted.h * (grow > 0.1 ? grow : 0.1));
        adjusted.cx += side * distance * tweaks->cascade[cascade_spacing] * layout_width / 1000.0;
        adjusted.angle += side * distance * tweaks->cascade[cascade_angle];
        adjusted.rotate_y += -side * distance * tweaks->cascade[cascade_tilt];
        adjusted.opacity = LV_CLAMP(0, adjusted.opacity - distance * tweaks->cascade[cascade_fade] * 255 / 100, 255);
    }

    tweak_box(tweak, &adjusted.cx, &adjusted.cy, &adjusted.w, &adjusted.h);
    adjusted.angle = style_tweak_get(tweak, tweak_angle, adjusted.angle);
    adjusted.rotate_x = style_tweak_get(tweak, tweak_tilt_x, adjusted.rotate_x);
    adjusted.rotate_y = style_tweak_get(tweak, tweak_tilt_y, adjusted.rotate_y);
    adjusted.opacity = style_tweak_get(tweak, tweak_opacity, adjusted.opacity);
    adjusted.glyph = style_tweak_get(tweak, tweak_glyph, adjusted.glyph);

    const style_slot *slot = &adjusted;
    const double cx = layout_mirror ? layout_width - slot->cx : slot->cx;
    const int flip = layout_mirror ? -1 : 1;
    style_json_add(
        "%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d,\"rotate_x\":%d,\"rotate_y\":%d,\"angle\":%d,"
        "\"opacity\":%d,\"glyph\":%d%s%s%s}",
        index ? "," : "", (int) lround(cx - slot->w / 2.0), (int) lround(slot->cy - slot->h / 2.0), slot->w, slot->h,
        slot->radius, slot->rotate_x, slot->rotate_y * flip, slot->angle * flip, slot->opacity, slot->glyph,
        slot->label ? ",\"label\":\"" : "", slot->label ? slot->label : "", slot->label ? "\"" : ""
    );
}

void style_add_glyph_item(
    const int index, const double x, const double y, const int w, const int h, const int radius, const int glyph,
    const char *label
) {
    const int item = menu_item_order()[index];
    if (index >= items_written) items_written = index + 1;
    const style_tweak *tweak = &style_tweaks_current()->items[item < TWEAK_ITEMS ? item : 0];
    double cx = x + w / 2.0, cy = y + h / 2.0;
    int width = w, height = h;
    tweak_box(tweak, &cx, &cy, &width, &height);

    const double left = layout_mirror ? layout_width - cx - width / 2.0 : cx - width / 2.0;
    style_json_add(
        "%s\"%s\":{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"radius\":%d,\"glyph\":%d,\"angle\":%d,\"rotate_x\":%d,"
        "\"rotate_y\":%d,\"depth\":%d,\"opacity\":%d%s%s%s}",
        index ? "," : "", item_names[item], (int) lround(left), (int) lround(cy - height / 2.0), width, height, radius,
        style_tweak_get(tweak, tweak_glyph, glyph), style_tweak_get(tweak, tweak_angle, 0),
        style_tweak_get(tweak, tweak_tilt_x, 0), style_tweak_get(tweak, tweak_tilt_y, 0),
        style_tweak_get(tweak, tweak_depth, 0), style_tweak_get(tweak, tweak_opacity, 255),
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
    slot_middle = (rows > columns ? rows : columns) / 2;
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
    theme->grid.current_item_label.offset_x = (int16_t) ((layout_mirror ? width - x : x) - width / 2.0);
    theme->grid.current_item_label.offset_y = (int16_t) (y - height / 2.0);
}

int style_content_bottom(const struct theme_config *theme, const int height) {
    if (!config.settings.themeopt.menu_label) return height - theme->footer.height;

    const int label = theme->grid.current_item_label.height + abs(theme->grid.current_item_label.offset_y);
    return height - (label > theme->footer.height ? label : theme->footer.height);
}

int style_slots_written(void) {
    return slots_written;
}

int style_items_written(void) {
    return items_written;
}
