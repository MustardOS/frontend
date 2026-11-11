#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <common/runtime/init.h>
#include <common/ui/common.h>
#include <common/ui/grid.h>
#include <common/ui/font.h>
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
#include <common/ui/nav/cell_art.h>
#include <common/display/theme_json.h>

static char layout_key[MAX_BUFFER_SIZE];
static char *layout_json = NULL;

const char *grid_layout_json(void) {
    if (menu_style_active()) return menu_style_layout_json();

    char key[MAX_BUFFER_SIZE];
    char alt_name[MAX_BUFFER_SIZE];
    get_active_alternate(alt_name, sizeof(alt_name));
    snprintf(key, sizeof(key), "%s|%s|%s|%s", theme_base, mux_dim, mux_module, alt_name);
    if (strcmp(key, layout_key) == 0) return layout_json;

    snprintf(layout_key, sizeof(layout_key), "%s", key);
    char alt_base[MAX_BUFFER_SIZE];
    const int has_alt = get_alt_base_path(alt_base, sizeof(alt_base));
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

    for (size_t b = 0; b < A_SIZE(bases) && !layout_json; b++) {
        if (bases[b]) layout_json = theme_json_module_layout(bases[b], mux_module);
    }

    return layout_json;
}

int grid_layout_full_names(void) {
    const char *data = grid_layout_json();
    if (!data) return 0;

    const struct json text = json_object_get(json_parse(data), "text");
    return json_string_compare(text, "across") == 0 || json_string_compare(text, "upwards") == 0
           || json_string_compare(text, "none") == 0;
}

static int layout_value(const struct json entry, const char *key, const int fallback) {
    const struct json value = json_object_get(entry, key);
    return json_exists(value) ? json_int(value) : fallback;
}

static int layout_flag(const struct json entry, const struct json root, const char *key) {
    struct json value = json_object_get(entry, key);
    if (!json_exists(value)) value = json_object_get(root, key);
    if (!json_exists(value)) return 1;
    if (json_type(value) == JSON_TRUE || json_type(value) == JSON_FALSE) return json_bool(value);
    return json_int(value) != 0;
}

#define LAYOUT_CACHE_SIZE 16

typedef struct {
    int slot;
    char name[64];
    grid_layout_entry entry;
} layout_cache_entry;

static layout_cache_entry layout_cache[LAYOUT_CACHE_SIZE];
static int layout_cache_count = 0;

void grid_layout_forget(void) {
    layout_cache_count = 0;
}

static grid_layout_entry find_grid_layout_uncached(int item_index, int panel_index);

grid_layout_entry find_grid_layout(const int item_index, const int panel_index) {
    const int slot = is_carousel_grid_mode() ? panel_index : -1;
    const int named = slot < 0 && item_index >= 0 && (size_t) item_index < item_count && items[item_index].glyph_icon;
    const char *name = named ? items[item_index].glyph_icon : "";

    for (int i = 0; i < layout_cache_count; i++) {
        const layout_cache_entry *cached = &layout_cache[i];
        if (cached->slot == slot && (slot >= 0 || strcmp(cached->name, name) == 0)) return cached->entry;
    }

    const grid_layout_entry entry = find_grid_layout_uncached(item_index, panel_index);
    if (layout_cache_count < LAYOUT_CACHE_SIZE && (slot >= 0 || named) && strlen(name) < sizeof(layout_cache[0].name)) {
        layout_cache_entry *cached = &layout_cache[layout_cache_count++];
        cached->slot = slot;
        snprintf(cached->name, sizeof(cached->name), "%s", name);
        cached->entry = entry;
    }
    return entry;
}

static grid_layout_entry find_grid_layout_uncached(const int item_index, const int panel_index) {
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
    out.radius = LV_CLAMP(0, layout_value(entry, "radius", theme.grid.cell.radius), LV_MIN(out.w, out.h) / 2);
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

    const int radius_pct = layout_value(entry, "radius_pct", layout_value(root, "radius_pct", -1));
    if (radius_pct >= 0) out.radius = LV_MIN(out.w, out.h) / 2 * LV_MIN(radius_pct, 100) / 100;
    out.text_size = layout_value(entry, "text_size", layout_value(root, "text_size", 100));

    struct json art = json_object_get(entry, "art");
    if (!json_exists(art)) art = json_object_get(root, "art");
    out.art = json_string_compare(art, "cover") == 0 ? 1 : json_string_compare(art, "native") == 0 ? 2 : 0;
    out.slant = layout_value(entry, "slant", layout_value(root, "slant", 0));

    struct json text = json_object_get(entry, "text");
    if (!json_exists(text)) text = json_object_get(root, "text");
    out.text_mode = json_string_compare(text, "across") == 0 ? 1 : json_string_compare(text, "upwards") == 0 ? 2 : 0;
    const int upwards = out.text_mode == 2;
    const int rise = upwards ? (int) lround(90.0 - atan2((double) out.slant, (double) out.h) * 180.0 / M_PI) : 0;
    out.text_x = layout_value(entry, "text_x", layout_value(root, "text_x", 0));
    out.text_y = layout_value(entry, "text_y", layout_value(root, "text_y", upwards ? out.h / 2 : out.h * 86 / 100));
    out.text_angle = layout_value(entry, "text_angle", layout_value(root, "text_angle", rise));
    out.shadow = layout_flag(entry, root, "shadow");
    out.border = layout_flag(entry, root, "border");
    out.text_bg = LV_CLAMP(0, layout_value(entry, "text_bg", layout_value(root, "text_bg", 120)), 255);
    out.label_width = layout_value(entry, "label_width", layout_value(root, "label_width", out.w * 7 / 4));
    out.label_gap = layout_value(entry, "label_gap", layout_value(root, "label_gap", out.h / 12));
    return out;
}

static void label_below_ext_cb(lv_event_t *e) {
    lv_event_set_ext_draw_size(e, (lv_coord_t) (intptr_t) lv_event_get_user_data(e));
}

static void slice_label(lv_obj_t *panel, const grid_layout_entry *entry, const int focused) {
    lv_obj_t *label = lv_obj_get_child(panel, 1);
    if (!label) return;

    const int item = (int) IFU(lv_obj_get_user_data(panel));
    if (item >= 0 && (size_t) item < item_count) lv_label_set_text(label, items[item].display_name);

    const int slant = abs(entry->slant);
    const int length = abs(entry->text_angle) > 45 ? entry->h * 7 / 10 : entry->w - slant - 16;
    lv_obj_set_size(label, (lv_coord_t) (length > 16 ? length : 16), LV_SIZE_CONTENT);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_white(), MU_OBJ_MAIN_FOCUS);

    const lv_opa_t shown = focused ? LV_OPA_COVER : LV_OPA_TRANSP;
    lv_obj_set_style_text_opa(label, shown, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, shown, MU_OBJ_MAIN_FOCUS);
    lv_obj_set_style_bg_color(label, lv_color_black(), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(label, (lv_opa_t) (focused ? entry->text_bg : 0), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(label, 6, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_ver(label, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_hor(label, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_y(label, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_update_layout(label);

    const double row = entry->h > 0 ? (double) entry->text_y / entry->h : 0.5;
    const int lean = (int) lround(entry->slant * (0.5 - row));
    const int half = lv_obj_get_height(label) / 2;
    lv_obj_align(label, LV_ALIGN_TOP_MID, (lv_coord_t) (lean + entry->text_x), (lv_coord_t) (entry->text_y - half));
    lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_transform_pivot_y(label, (lv_coord_t) half, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_transform_angle(label, (lv_coord_t) (-entry->text_angle * 10), MU_OBJ_MAIN_DEFAULT);
}

void place_grid_layout(lv_obj_t *cell_panel, const grid_layout_entry *entry, const int step) {
    const int is_focus = step == 0;
    const int gap = is_carousel_grid_mode() ? theme.grid.layout_slot_gap * step : 0;
    lv_obj_add_flag(cell_panel, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_pos(cell_panel, (lv_coord_t) (entry->x + gap), (lv_coord_t) entry->y);

    if (entry->opacity < 255) {
        const int opacity = entry->opacity < 0 ? 0 : entry->opacity;
        cell_set_opa(cell_panel, (lv_opa_t) (cell_opa(cell_panel) * opacity / 255));
    }

    if (entry->glyph > LV_MIN(entry->w, entry->h)) {
        lv_obj_add_flag(cell_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_style_clip_corner(cell_panel, false, MU_OBJ_MAIN_DEFAULT);
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

    if (entry->art) {
        const int item = (int) IFU(lv_obj_get_user_data(cell_panel));
        const char *name = item >= 0 && (size_t) item < item_count ? items[item].glyph_icon : NULL;
        cell_art_apply(cell_panel, name, entry->w, entry->h, entry->slant, is_focus, entry->art == 2);
        if (entry->text_mode) slice_label(cell_panel, entry, is_focus);
    }

    lv_obj_t *sized_label = lv_obj_get_child(cell_panel, 1);
    if (sized_label && entry->text_size > 0 && entry->text_size != 100) {
        load_font_section_scaled(FONT_PANEL_DIR, sized_label, entry->text_size);
    }

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

            const lv_coord_t line = lv_font_get_line_height(lv_obj_get_style_text_font(cell_label, LV_PART_MAIN));
            const int below = entry->label_gap + line * 2 + 4;
            const int beside = (entry->label_width - entry->w) / 2 + 4;
            lv_obj_add_event_cb(
                cell_panel, label_below_ext_cb, LV_EVENT_REFR_EXT_DRAW_SIZE,
                (void *) (intptr_t) (below > beside ? below : beside)
            );
            lv_obj_refresh_ext_draw_size(cell_panel);
        }
    }

    if (!entry->border) {
        lv_obj_set_style_border_width(cell_panel, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(cell_panel, 0, MU_OBJ_MAIN_FOCUS);
    }
    if (!entry->shadow) {
        lv_obj_set_style_shadow_width(cell_panel, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_shadow_width(cell_panel, 0, MU_OBJ_MAIN_FOCUS);
    }

    if (is_focus) lv_obj_move_foreground(cell_panel);
}
