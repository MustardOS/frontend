#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/grid.h>
#include <common/runtime/log.h>
#include <common/display/language.h>
#include <common/display/theme.h>
#include <common/config/config.h>
#include <common/content/core/common.h>
#include <module/muxshare.h>
#include <common/storage/fileio.h>
#include <json/json.h>
#include <common/ui/nav/grid_layout.h>
#include <common/ui/nav/menu_style.h>

static char layout_key[MAX_BUFFER_SIZE];
static char *layout_json = NULL;

const char *grid_layout_json(void) {
    if (menu_style_active()) return menu_style_layout_json();

    char key[MAX_BUFFER_SIZE];
    char alt_base[MAX_BUFFER_SIZE];
    const int has_alt = get_alt_base_path(alt_base, sizeof(alt_base));
    snprintf(key, sizeof(key), "%s|%s|%s|%s", theme_base, mux_dim, mux_module, has_alt ? alt_base : "");
    if (strcmp(key, layout_key) == 0) return layout_json;

    snprintf(layout_key, sizeof(layout_key), "%s", key);
    free(layout_json);
    layout_json = NULL;

    const char *bases[] = {has_alt ? alt_base : NULL, theme_base};
    const char *dims[] = {mux_dim, ""};
    char path[MAX_BUFFER_SIZE];
    for (size_t b = 0; b < A_SIZE(bases) && !layout_json; b++) {
        if (!bases[b]) continue;
        for (size_t d = 0; d < A_SIZE(dims) && !layout_json; d++) {
            snprintf(path, sizeof(path), "%s/%slayout/%s.json", bases[b], dims[d], mux_module);
            if (file_exist(path)) {
                layout_json = read_all_char_from(path);
                if (layout_json && !json_valid(layout_json)) {
                    LOG_WARN(mux_module, "Ignoring invalid theme layout: %s", path);
                    free(layout_json);
                    layout_json = NULL;
                }
            }
        }
    }

    return layout_json;
}

static int layout_value(const struct json entry, const char *key, const int fallback) {
    const struct json value = json_object_get(entry, key);
    return json_exists(value) ? json_int(value) : fallback;
}

grid_layout_entry find_grid_layout(const int item_index, const int panel_index) {
    grid_layout_entry out = {0};
    const char *data = grid_layout_json();
    if (!data) return out;

    const struct json root = json_parse(data);
    const struct json mode = json_object_get(root, "mode");
    struct json entry = {0};

    if (json_string_compare(mode, "slots") == 0 && is_carousel_grid_mode()) {
        entry = json_array_get(json_object_get(root, "slots"), (size_t) panel_index);
    } else if (json_string_compare(mode, "items") == 0 && items[item_index].glyph_icon) {
        entry = json_object_get(json_object_get(root, "items"), items[item_index].glyph_icon);
    }
    if (!json_exists(entry)) return out;

    out.found = 1;
    out.x = layout_value(entry, "x", 0);
    out.y = layout_value(entry, "y", 0);
    out.w = layout_value(entry, "w", theme.grid.cell.width);
    out.h = layout_value(entry, "h", theme.grid.cell.height);
    out.radius = layout_value(entry, "radius", theme.grid.cell.radius);
    out.glyph = layout_value(entry, "glyph", 0);
    out.opacity = layout_value(entry, "opacity", 255);
    out.angle = layout_value(entry, "angle", 0);
    out.rotate_x = layout_value(entry, "rotate_x", 0);
    out.rotate_y = layout_value(entry, "rotate_y", 0);
    out.depth = layout_value(entry, "depth", 0);
    out.perspective = layout_value(entry, "perspective", layout_value(root, "perspective", 0));
    out.reflect = layout_value(entry, "reflect", layout_value(root, "reflect", 0));

    struct json label = json_object_get(entry, "label");
    if (!json_exists(label)) label = json_object_get(root, "label");
    out.label_below = json_string_compare(label, "below") == 0;
    out.label_none = json_string_compare(label, "none") == 0;
    out.label_beside = json_string_compare(label, "beside") == 0;
    out.label_width = layout_value(entry, "label_width", layout_value(root, "label_width", out.w * 7 / 4));
    out.label_gap = layout_value(entry, "label_gap", layout_value(root, "label_gap", out.h / 12));
    return out;
}

void place_grid_layout(lv_obj_t *cell_panel, const grid_layout_entry *entry, const int step) {
    const int is_focus = step == 0;
    const int gap = is_carousel_grid_mode() ? theme.grid.layout_slot_gap * step : 0;
    lv_obj_add_flag(cell_panel, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(cell_panel, (lv_coord_t) (entry->x + gap), (lv_coord_t) entry->y);

    if (entry->opacity < 255) {
        const lv_opa_t base = lv_obj_get_style_opa_layered(cell_panel, LV_PART_MAIN);
        const int opacity = entry->opacity < 0 ? 0 : entry->opacity;
        lv_obj_set_style_opa_layered(cell_panel, (lv_opa_t) (base * opacity / 255), MU_OBJ_MAIN_DEFAULT);
    }

    if (entry->angle != 0) {
        lv_obj_set_style_transform_pivot_x(cell_panel, (lv_coord_t) (entry->w / 2), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_pivot_y(cell_panel, (lv_coord_t) (entry->h / 2), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_angle(cell_panel, (lv_coord_t) (entry->angle * 10), MU_OBJ_MAIN_DEFAULT);
    }

    const int reflect = entry->reflect < 0 ? 0 : entry->reflect > 255 ? 255 : entry->reflect;
    lv_obj_set_transform_3d(
        cell_panel, (int16_t) (entry->rotate_x * 10), (int16_t) (entry->rotate_y * 10), (int16_t) entry->depth,
        (int16_t) (entry->perspective < 0 ? 0 : entry->perspective), (lv_opa_t) reflect
    );

    if (entry->label_none) {
        lv_obj_t *cell_image = lv_obj_get_child(cell_panel, 0);
        lv_obj_t *cell_label = lv_obj_get_child(cell_panel, 1);
        lv_obj_t *cell_image_focused = lv_obj_get_child(cell_panel, 2);

        if (cell_label) {
            lv_obj_set_style_text_opa(cell_label, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_text_opa(cell_label, LV_OPA_TRANSP, MU_OBJ_MAIN_FOCUS);
        }
        if (cell_image) lv_obj_align(cell_image, LV_ALIGN_CENTER, 0, 0);
        if (cell_image_focused) lv_obj_align(cell_image_focused, LV_ALIGN_CENTER, 0, 0);
    }

    if (entry->label_beside) {
        lv_obj_t *cell_image = lv_obj_get_child(cell_panel, 0);
        lv_obj_t *cell_label = lv_obj_get_child(cell_panel, 1);
        lv_obj_t *cell_image_focused = lv_obj_get_child(cell_panel, 2);

        const int pad = entry->h / 6;
        const int glyph = entry->glyph > 0 ? entry->glyph : entry->h * 3 / 5;
        if (cell_image) lv_obj_align(cell_image, LV_ALIGN_LEFT_MID, (lv_coord_t) pad, 0);
        if (cell_image_focused) lv_obj_align(cell_image_focused, LV_ALIGN_LEFT_MID, (lv_coord_t) pad, 0);

        if (cell_label && lv_obj_get_style_width(cell_label, LV_PART_MAIN) > 0) {
            lv_obj_set_width(cell_label, (lv_coord_t) (entry->w - glyph - pad * 3));
            lv_label_set_long_mode(cell_label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(cell_label, LV_TEXT_ALIGN_LEFT, MU_OBJ_MAIN_DEFAULT);
            lv_obj_align(cell_label, LV_ALIGN_LEFT_MID, (lv_coord_t) (glyph + pad * 2), 0);
        }
    }

    if (entry->label_below) {
        lv_obj_t *cell_image = lv_obj_get_child(cell_panel, 0);
        lv_obj_t *cell_label = lv_obj_get_child(cell_panel, 1);
        lv_obj_t *cell_image_focused = lv_obj_get_child(cell_panel, 2);

        lv_obj_add_flag(cell_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        if (cell_image) lv_obj_align(cell_image, LV_ALIGN_CENTER, 0, 0);
        if (cell_image_focused) lv_obj_align(cell_image_focused, LV_ALIGN_CENTER, 0, 0);

        if (cell_label && lv_obj_get_style_width(cell_label, LV_PART_MAIN) > 0) {
            const int pad_top = lv_obj_get_style_pad_top(cell_panel, LV_PART_MAIN);
            const int border = lv_obj_get_style_border_width(cell_panel, LV_PART_MAIN);
            lv_obj_set_width(cell_label, (lv_coord_t) entry->label_width);
            lv_obj_align(
                cell_label, LV_ALIGN_TOP_MID, 0, (lv_coord_t) (entry->h - pad_top - border + entry->label_gap)
            );
        }
    }

    if (is_focus) lv_obj_move_foreground(cell_panel);
}
