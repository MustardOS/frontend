#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <json/json.h>
#include <common/runtime/init.h>
#include <common/storage/fileio.h>
#include <common/ui/nav/grid_nav.h>
#include <common/display/theme_json.h>
#include <common/ui/nav/content_layout.h>

#define LAYOUT_JSON_SIZE   2048
#define LAYOUT_OFF(MEMBER) offsetof(content_layout_config, MEMBER)

static const char *const content_modules[content_layout_count] = {"muxplore", "muxcollect", "muxhistory"};

const char *const content_layout_screens[content_layout_count] = {"explore", "collection", "history"};

const content_layout_field content_layout_fields[content_field_count] = {
    [content_field_style] = {"style", LAYOUT_OFF(style), 0, content_style_count - 1, 1, content_style_theme},
    [content_field_columns] = {"columns", LAYOUT_OFF(columns), 2, 8, 1, 4},
    [content_field_rows] = {"rows", LAYOUT_OFF(rows), 1, 5, 1, 2},
    [content_field_size] = {"size", LAYOUT_OFF(size), 50, 100, 5, 90},
    [content_field_names] = {"names", LAYOUT_OFF(names), 0, content_names_count - 1, 1, content_names_bar},
    [content_field_strip_columns] = {"strip_columns", LAYOUT_OFF(strip_columns), 2, 8, 1, 4},
    [content_field_strip_size] = {"strip_size", LAYOUT_OFF(strip_size), 50, 100, 5, 90},
    [content_field_side] = {"side", LAYOUT_OFF(side), 0, content_side_count - 1, 1, content_side_full},
    [content_field_offset_x] = {"offset_x", LAYOUT_OFF(offset_x), -200, 200, 2, 0},
    [content_field_offset_y] = {"offset_y", LAYOUT_OFF(offset_y), -200, 200, 2, 0},
    [content_field_count_items] = {"count", LAYOUT_OFF(count), 0, 30, 1, 0},
    [content_field_spacing] = {"spacing", LAYOUT_OFF(spacing), -1, 32, 1, -1},
    [content_field_counter_align] = {"counter_align", LAYOUT_OFF(counter_align), -1, 2, 1, -1},
    [content_field_counter_x] = {"counter_x", LAYOUT_OFF(counter_x), -200, 200, 2, 0},
    [content_field_counter_y] = {"counter_y", LAYOUT_OFF(counter_y), -200, 200, 2, 0},
};

static int name_bar_highlight;

static content_layout_config folder_layout;
static int folder_layout_set;

int16_t *content_layout_value(content_layout_config *layout, const int field) {
    return (int16_t *) ((char *) layout + content_layout_fields[field].offset);
}

static int16_t layout_get(const content_layout_config *layout, const int field) {
    return *(const int16_t *) ((const char *) layout + content_layout_fields[field].offset);
}

void content_layout_reset(content_layout_config *layout) {
    for (int i = 0; i < content_field_count; i++)
        *content_layout_value(layout, i) = (int16_t) content_layout_fields[i].fallback;
}

void content_layout_read(const struct json object, content_layout_config *layout) {
    for (int i = 0; i < content_field_count; i++) {
        const struct json value = json_object_get(object, content_layout_fields[i].key);
        if (json_type(value) != JSON_NUMBER) continue;

        const content_layout_field *field = &content_layout_fields[i];
        *content_layout_value(layout, i) = (int16_t) LV_CLAMP(field->min, json_int(value), field->max);
    }
}

size_t content_layout_write(
    char *out, const size_t size, const content_layout_config *layout, const content_layout_config *base
) {
    size_t used = 0;

    for (int i = 0; i < content_field_count && used < size; i++) {
        const int16_t value = layout_get(layout, i);
        if (value == layout_get(base, i)) continue;

        used += (size_t) snprintf(
            out + used, size - used, "%s\"%s\":%d", used ? "," : "", content_layout_fields[i].key, value
        );
    }

    return used;
}

int content_layout_store(const char *path, const char *document, const int empty) {
    if (empty) return !file_exist(path) || remove(path) == 0;

    create_directories(path, 1);
    return write_text_to_file_atomic(path, CHAR, document);
}

void content_layout_load(struct mux_config *config) {
    for (int i = 0; i < content_layout_count; i++)
        content_layout_reset(&config->settings.layout[i]);

    char *data = theme_json_read_file(CONTENT_LAYOUT_GLOBAL);
    if (!data) return;

    const struct json root = json_parse(data);
    for (int i = 0; i < content_layout_count; i++)
        content_layout_read(json_object_get(root, content_layout_screens[i]), &config->settings.layout[i]);

    free(data);
}

int content_layout_save(const struct mux_config *config) {
    content_layout_config base;
    content_layout_reset(&base);

    char document[LAYOUT_JSON_SIZE];
    size_t used = (size_t) snprintf(document, sizeof(document), "{");
    int screens = 0;

    for (int i = 0; i < content_layout_count && used < sizeof(document); i++) {
        char fields[LAYOUT_JSON_SIZE];
        if (!content_layout_write(fields, sizeof(fields), &config->settings.layout[i], &base)) continue;

        used += (size_t) snprintf(
            document + used, sizeof(document) - used, "%s\"%s\":{%s}", screens++ ? "," : "", content_layout_screens[i],
            fields
        );
    }

    if (used >= sizeof(document) - 2) return 0;
    snprintf(document + used, sizeof(document) - used, "}");

    return content_layout_store(CONTENT_LAYOUT_GLOBAL, document, !screens);
}

const content_layout_config *content_layout_current(const struct mux_config *config) {
    if (folder_layout_set && strcmp(mux_module, content_modules[content_layout_explore]) == 0) return &folder_layout;

    for (int i = 0; i < content_layout_count; i++) {
        if (strcmp(mux_module, content_modules[i]) == 0) return &config->settings.layout[i];
    }

    return NULL;
}

content_layout_config *content_layout_folder_config(void) {
    return &folder_layout;
}

void content_layout_folder_use(const int use) {
    folder_layout_set = use;
}

int content_layout_grid_forced(const struct mux_config *config) {
    const content_layout_config *layout = content_layout_current(config);
    return layout
           && (layout->style == content_style_grid || layout->style == content_style_carousel
               || layout->style == content_style_carousel_vertical);
}

int content_layout_side(const struct mux_config *config) {
    const content_layout_config *layout = content_layout_current(config);
    return content_layout_grid_forced(config) ? layout->side : content_side_full;
}

int content_layout_name_bar_highlight(void) {
    return name_bar_highlight;
}

int content_layout_mixed(const struct mux_config *config) {
    const content_layout_config *layout = content_layout_current(config);
    return layout && (layout->style == content_style_grid_list || layout->style == content_style_carousel_list);
}

int16_t content_layout_item_count(const struct mux_config *config) {
    const content_layout_config *layout = content_layout_current(config);
    return layout && layout->count > 0 ? layout->count : config->settings.themeopt.content_item_count;
}

int16_t content_layout_item_spacing(const struct mux_config *config) {
    const content_layout_config *layout = content_layout_current(config);
    return layout ? layout->spacing : -1;
}

static void borrow_list_colours(struct theme_config *theme) {
    theme->grid.cell_default.background = theme->list_default.background;
    theme->grid.cell_default.background_gradient_color = theme->list_default.background;
    theme->grid.cell_default.background_gradient_direction = 0;
    theme->grid.cell_default.background_alpha = LV_MAX(theme->list_default.background_alpha, 40);
    theme->grid.cell_default.border_alpha = 0;
    theme->grid.cell_default.text = theme->list_default.text;
    theme->grid.cell_default.image_recolour = theme->list_default.glyph_recolour;
    theme->grid.cell_default.image_recolour_alpha = theme->list_default.glyph_recolour_alpha;

    theme->grid.cell_focus.background = theme->list_focus.background;
    theme->grid.cell_focus.background_gradient_color = theme->list_focus.background;
    theme->grid.cell_focus.background_gradient_direction = 0;
    theme->grid.cell_focus.background_alpha = LV_MAX(theme->list_focus.background_alpha, 120);
    theme->grid.cell_focus.border = theme->list_focus.indicator;
    theme->grid.cell_focus.border_alpha = theme->list_focus.indicator_alpha;
    theme->grid.cell_focus.text = theme->list_focus.text;
    theme->grid.cell_focus.image_recolour = theme->list_focus.glyph_recolour;
    theme->grid.cell_focus.image_recolour_alpha = theme->list_focus.glyph_recolour_alpha;

    theme->grid.cell.border_width = 2;
    theme->grid.cell.shadow_width = 0;

    theme->grid.current_item_label.background = theme->list_focus.background;
    theme->grid.current_item_label.background_gradient_color = theme->list_focus.background;
    theme->grid.current_item_label.background_alpha = LV_MAX(theme->list_focus.background_alpha, 200);
    theme->grid.current_item_label.border_alpha = 0;
    theme->grid.current_item_label.shadow_width = 0;
    theme->grid.current_item_label.text = theme->list_focus.text;
    theme->grid.current_item_label.radius = theme->list_default.radius;
}

static void place_name_bar(struct theme_config *theme, const int width, const int label_height) {
    theme->grid.current_item_label.alignment = LV_ALIGN_BOTTOM_MID;
    theme->grid.current_item_label.width = (int16_t) (width * 9 / 10);
    theme->grid.current_item_label.height = (int16_t) label_height;
    theme->grid.current_item_label.offset_x = 0;
    theme->grid.current_item_label.offset_y = (int16_t) -(theme->footer.height + 4);
    theme->grid.current_item_label.label_long_mode = LV_LABEL_LONG_SCROLL_CIRCULAR;
    theme->grid.current_item_label.text_alpha = 255;
    theme->grid.current_item_label.text_alignment = LV_TEXT_ALIGN_CENTER;
    theme->grid.current_item_label.text_padding_left = 8;
    theme->grid.current_item_label.text_padding_right = 8;
}

static void hide_name_bar(struct theme_config *theme) {
    theme->grid.current_item_label.text_alpha = 0;
    theme->grid.current_item_label.background_alpha = 0;
    theme->grid.current_item_label.border_alpha = 0;
    theme->grid.current_item_label.shadow_width = 0;
}

static void apply_names(struct theme_config *theme, const content_layout_config *layout, const int cell) {
    const int in_cell = layout->names == content_names_cell;
    theme->grid.cell_default.text_alpha = (int16_t) (in_cell ? 255 : 0);
    theme->grid.cell_focus.text_alpha = (int16_t) (in_cell ? 255 : 0);
    theme->grid.cell.image_padding_top = (int16_t) (in_cell ? cell / 10 : 0);
    if (in_cell) {
        theme->glyph.grid = (int16_t) (cell / 2);
        theme->grid.cell.label_long_mode = LV_LABEL_LONG_DOT;
    }
    theme->grid.cell.text_padding_bottom = (int16_t) (cell / 16);
    theme->grid.cell.text_padding_side = (int16_t) (cell / 16);
}

void content_layout_apply(
    struct theme_config *theme, const struct mux_config *config, const int width, const int height
) {
    name_bar_highlight = 0;

    const content_layout_config *layout = content_layout_current(config);
    if (!layout || layout->style == content_style_theme) return;

    if (layout->style == content_style_list) {
        theme->grid.column_count = 0;
        theme->grid.row_count = 0;
        return;
    }

    const int borrowed = theme->grid.column_count <= 0 || theme->grid.row_count <= 0;
    if (borrowed) borrow_list_colours(theme);

    if (layout->style == content_style_grid_list || layout->style == content_style_carousel_list) {
        const int in_cell = layout->names != content_names_hidden;
        theme->grid.cell_default.text_alpha = (int16_t) (in_cell ? 255 : 0);
        theme->grid.cell_focus.text_alpha = (int16_t) (in_cell ? 255 : 0);
        theme->grid.cell.label_long_mode = LV_LABEL_LONG_DOT;
        theme->grid.column_count = 0;
        theme->grid.row_count = 0;
        return;
    }

    const int vertical = layout->style == content_style_carousel_vertical;
    const int carousel = vertical || layout->style == content_style_carousel;
    const int count = carousel ? LV_CLAMP(3, layout->columns | 1, 7) : layout->columns;
    const int columns = vertical ? 1 : count;
    const int rows = vertical ? count : carousel ? 1 : layout->rows;

    const int bar = layout->names == content_names_bar;
    const int label_height = theme->mux.item.height > 0 ? theme->mux.item.height : height / 12;

    const int area_width = layout->side == content_side_full ? width : width * 58 / 100;
    const int area_left = layout->side == content_side_right ? width - area_width : 0;

    const int top = theme->header.height + 2;
    const int bottom = height - theme->footer.height - (bar ? label_height + 8 : 0);
    const int area = bottom - top;

    const int column_width = area_width / columns;
    const int row_height = area / rows;
    const int slot = column_width < row_height ? column_width : row_height;
    const int cell = slot * layout->size / 100;

    theme->grid.row_count = (int16_t) rows;
    theme->grid.column_count = (int16_t) columns;
    theme->grid.column_width = (int16_t) column_width;
    theme->grid.row_height = (int16_t) row_height;
    theme->grid.column_padding = 0;
    theme->grid.row_padding = 0;
    theme->grid.alignment = 0;
    theme->grid.location_x = (int16_t) (area_left + (area_width - column_width * columns) / 2 + layout->offset_x);
    theme->grid.location_y = (int16_t) (top + (area - row_height * rows) / 2 + layout->offset_y);
    theme->grid.navigation_type = vertical ? nav_standard : nav_horizontal;
    theme->grid.carousel_fade = (int16_t) (carousel ? 30 : 0);
    theme->grid.layout_slot_gap = 0;

    theme->grid.cell.width = (int16_t) cell;
    theme->grid.cell.height = (int16_t) cell;
    theme->grid.cell.column_align = LV_GRID_ALIGN_CENTER;
    theme->grid.cell.row_align = LV_GRID_ALIGN_CENTER;
    theme->grid.cell.radius = (int16_t) (cell / 10);
    theme->grid.cell.clip_corner = 1;

    apply_names(theme, layout, cell);

    if (bar) {
        place_name_bar(theme, area_width, label_height);
        theme->grid.current_item_label.offset_x = (int16_t) (area_left + area_width / 2 - width / 2);
        name_bar_highlight = borrowed;
    } else {
        hide_name_bar(theme);
    }
}
