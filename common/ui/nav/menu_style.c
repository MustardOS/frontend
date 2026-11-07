#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <common/storage/fileio.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/display/theme.h>
#include <common/ui/image.h>
#include <common/display/theme_json.h>
#include <mini/mini.h>
#include <common/ui/nav/grid_nav.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tweak.h>
#include <common/ui/nav/style/grid.h>
#include <common/ui/nav/style/carousel.h>
#include <common/ui/nav/style/coverflow.h>
#include <common/ui/nav/style/bubbles.h>
#include <common/ui/nav/style/orbit.h>
#include <common/ui/nav/style/dial.h>
#include <common/ui/nav/style/stack.h>
#include <common/ui/nav/style/clock.h>
#include <common/ui/nav/style/rolodex.h>
#include <common/ui/nav/style/dock.h>
#include <common/ui/nav/style/fan.h>
#include <common/ui/nav/style/honeycomb.h>
#include <common/ui/nav/style/tiles.h>
#include <common/ui/nav/style/sidebar.h>
#include <common/ui/nav/style/fluent.h>
#include <common/ui/nav/style/spotlight.h>
#include <common/ui/nav/style/banners.h>
#include <common/ui/nav/style/crossbar.h>
#include <common/ui/nav/style/desktop.h>
#include <common/ui/nav/style/tabbar.h>
#include <common/ui/nav/style/paged.h>
#include <common/ui/nav/style/slices.h>
#include <common/ui/nav/style/mascot.h>

typedef void (*menu_style_fn)(struct theme_config *theme, int width, int height);

static const menu_style_fn style_apply[] = {
    [menu_style_grid] = style_grid_apply,
    [menu_style_carousel] = style_carousel_apply,
    [menu_style_coverflow] = style_coverflow_apply,
    [menu_style_bubbles] = style_bubbles_apply,
    [menu_style_orbit] = style_orbit_apply,
    [menu_style_dial] = style_dial_apply,
    [menu_style_stack] = style_stack_apply,
    [menu_style_clock] = style_clock_apply,
    [menu_style_rolodex] = style_rolodex_apply,
    [menu_style_dock] = style_dock_apply,
    [menu_style_fan] = style_fan_apply,
    [menu_style_honeycomb] = style_honeycomb_apply,
    [menu_style_tiles] = style_tiles_apply,
    [menu_style_sidebar] = style_sidebar_apply,
    [menu_style_fluent] = style_fluent_apply,
    [menu_style_spotlight] = style_spotlight_apply,
    [menu_style_banners] = style_banners_apply,
    [menu_style_crossbar] = style_crossbar_apply,
    [menu_style_desktop] = style_desktop_apply,
    [menu_style_tabbar] = style_tabbar_apply,
    [menu_style_paged] = style_paged_apply,
    [menu_style_slices] = style_slices_apply,
    [menu_style_mascot] = style_mascot_apply,
};

static int theme_allows_styles = 1;
static int card_art = 0;

static int detect_card_art(void) {
    char path[MAX_BUFFER_SIZE];
    if (!load_element_image_specifics(mux_dim, "muxlaunch", "grid", "explore", "default", "png", path, sizeof(path))) {
        return 0;
    }

    char embed[MAX_BUFFER_SIZE];
    snprintf(embed, sizeof(embed), "M:%s", path);
    lv_img_header_t header;
    if (lv_img_decoder_get_info(embed, &header) != LV_RES_OK || !header.w || !header.h) return 0;

    return header.w * 4 < header.h * 3 || header.h * 4 < header.w * 3;
}

int menu_style_card_art(void) {
    return menu_style_active() && card_art;
}

#define OPT(NAME) (1 << layout_##NAME)

_Static_assert(LAYOUT_OPTION_COUNT < (int) sizeof(((style_tweaks *) 0)->options), "options outgrew the saved string");
_Static_assert(LAYOUT_OPTION_COUNT < 31, "layout options outgrew the option mask");
#define NAME_OPTS (OPT(namex) | OPT(namey) | OPT(nameangle) | OPT(namebg) | OPT(namewidth) | CELL_OPTS)
#define CELL_OPTS (OPT(shape) | OPT(glyphscale))

static const int style_options[] = {
    [menu_style_list] = OPT(position) | OPT(glyph) | OPT(fit),
    [menu_style_grid] = OPT(size) | OPT(spacing) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_carousel] = OPT(size) | OPT(spacing) | NAME_OPTS,
    [menu_style_coverflow] = OPT(size) | OPT(spacing) | OPT(tilt) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_bubbles] = OPT(size) | OPT(swell) | NAME_OPTS,
    [menu_style_orbit] = OPT(size) | OPT(spacing) | OPT(tilt) | NAME_OPTS,
    [menu_style_dial] = OPT(size) | OPT(spacing) | NAME_OPTS,
    [menu_style_stack] = OPT(size) | OPT(position) | OPT(pile) | NAME_OPTS,
    [menu_style_clock] = OPT(size) | OPT(spacing) | OPT(swell) | NAME_OPTS,
    [menu_style_rolodex] = OPT(size) | OPT(tilt) | OPT(position) | NAME_OPTS,
    [menu_style_dock] = OPT(size) | OPT(spacing) | NAME_OPTS,
    [menu_style_fan] = OPT(size) | OPT(spacing) | OPT(tilt) | NAME_OPTS,
    [menu_style_honeycomb] = OPT(size) | OPT(spacing) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_tiles] = OPT(spacing) | NAME_OPTS,
    [menu_style_sidebar] = OPT(size) | OPT(position) | NAME_OPTS,
    [menu_style_fluent] = OPT(spacing) | OPT(large) | CELL_OPTS,
    [menu_style_spotlight] = OPT(size) | OPT(spacing) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_banners] = OPT(size) | OPT(tilt) | OPT(position) | CELL_OPTS,
    [menu_style_crossbar] = OPT(size) | OPT(spacing) | OPT(position) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_desktop] = OPT(size) | OPT(position) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_tabbar] = OPT(size) | NAME_OPTS,
    [menu_style_paged] = OPT(size) | OPT(spacing) | OPT(names) | OPT(text) | NAME_OPTS,
    [menu_style_slices] = OPT(focus) | OPT(gap) | OPT(slant) | OPT(slicetext) | OPT(textx) | OPT(texty) | OPT(textangle)
                          | OPT(textbg) | OPT(text),
    [menu_style_mascot] =
        OPT(size) | OPT(spacing) | OPT(perch) | OPT(mascotscale) | OPT(bounce) | OPT(names) | OPT(text) | NAME_OPTS,
};

static const char *const style_keys[] = {
    "theme",   "list",     "grid",    "carousel", "coverflow", "bubbles", "orbit",   "dial",   "stack",
    "clock",   "rolodex",  "dock",    "fan",      "honeycomb", "tiles",   "sidebar", "fluent", "spotlight",
    "banners", "crossbar", "desktop", "tabbar",   "paged",     "slices",  "mascot",
};

static int layout_values[LAYOUT_OPTION_COUNT];
static int layout_defaults[LAYOUT_OPTION_COUNT];
static int layout_editing = 0;

static int chosen_style = 0;

int menu_style_current(void) {
    return chosen_style;
}

static int current_style(void) {
    const int style = chosen_style;
    return style > 0 && style < (int) (sizeof(style_options) / sizeof(style_options[0])) ? style : 0;
}

const char *menu_style_key(const int style) {
    return style >= 0 && style < (int) (sizeof(style_keys) / sizeof(style_keys[0])) ? style_keys[style] : "theme";
}

static void apply_options(const char *options, int *values) {
    for (int i = 0; options[i] && i < LAYOUT_OPTION_COUNT; i++) {
        if (options[i] >= '0' && options[i] < '0' + menu_layout_choices(i)) values[i] = options[i] - '0';
    }
}

static void layout_path(char *path, const size_t size) {
    snprintf(path, size, CONF_CONFIG_PATH "settings/menu_layout/%d", current_style());
}

static void layout_load(const struct theme_config *theme) {
    const int style = current_style();
    for (int i = 0; i < LAYOUT_OPTION_COUNT; i++)
        layout_values[i] = 1;
    layout_values[layout_pile] = 0;
    layout_values[layout_names] = style == menu_style_desktop ? 1 : 0;
    layout_values[layout_fit] = theme->misc.content.size_to_content ? 1 : 0;
    layout_values[layout_glyph] = 0;
    layout_values[layout_perch] = style == menu_style_mascot ? 1 : 0;
    layout_values[layout_shape] = 0;
    if (style == menu_style_list) layout_values[layout_position] = LV_CLAMP(0, theme->misc.content.alignment, 2);
    if (style == menu_style_banners || style == menu_style_rolodex || style == menu_style_sidebar
        || style == menu_style_desktop || style == menu_style_crossbar) {
        layout_values[layout_position] = 0;
    }

    const style_tweaks *package = style_tweaks_package();
    if (package) apply_options(package->options, layout_values);
    for (int i = 0; i < LAYOUT_OPTION_COUNT; i++)
        layout_defaults[i] = layout_values[i];

    const char *options = style_tweaks_current()->options;
    char path[MAX_BUFFER_SIZE];
    layout_path(path, sizeof(path));
    const int legacy = !options[0] && style_tweaks_source() != tweak_target_package;
    char *saved = legacy && file_exist(path) ? read_all_char_from(path) : NULL;
    apply_options(saved ? saved : options, layout_values);
    free(saved);
}

int menu_layout_options(void) {
    return menu_style_active() ? style_options[current_style()] : 0;
}

int menu_layout_value(const int option) {
    return layout_values[option];
}

void menu_layout_edit(const int editing) {
    layout_editing = editing;
}

static int layout_allowed(const int option, const int value) {
    if (option == layout_large || option == layout_fit || option == layout_glyph || option == layout_slicetext) {
        return value < 2;
    }
    if (option != layout_position) return 1;

    const int style = current_style();
    if (style == menu_style_crossbar) return value < 2;
    if (style == menu_style_stack || style == menu_style_list || style == menu_style_desktop) return 1;
    return value != 1;
}

void menu_layout_default(const int option) {
    layout_values[option] = layout_defaults[option];
}

int menu_layout_choices(const int option) {
    return option == layout_shape ? 4 : 3;
}

void menu_layout_step(const int option, const int direction) {
    const int count = menu_layout_choices(option);
    int value = layout_values[option];
    do {
        value = (value + direction + count) % count;
    } while (!layout_allowed(option, value));
    layout_values[option] = value;
}

int menu_layout_save(const int target) {
    char *saved = style_tweaks_current()->options;
    const int offered = style_options[current_style()];
    for (int i = 0; i < LAYOUT_OPTION_COUNT; i++)
        saved[i] = offered & (1 << i) ? (char) ('0' + layout_values[i]) : '-';
    saved[LAYOUT_OPTION_COUNT] = '\0';
    if (target != tweak_target_global) return 1;

    char path[MAX_BUFFER_SIZE];
    layout_path(path, sizeof(path));
    create_directories(CONF_CONFIG_PATH "settings/menu_layout", 0);
    return write_text_to_file_atomic(path, CHAR, saved);
}

int menu_layout_reset_all(void) {
    const char *options = CONF_CONFIG_PATH "settings/menu_layout";
    const int removed = !dir_exist(options) || remove_directory_recursive(options) == 0;
    return style_tweaks_remove_all() && removed;
}

static void cell_shape(const int style) {
    static const char *const shapes[] = {
        NULL, "\"shape\":\"square\",", "\"shape\":\"round\",", "\"shape\":\"hexagon\","
    };
    const int shape = layout_values[layout_shape];
    if (!(style_options[style] & OPT(shape)) || shape <= 0 || shape >= (int) A_SIZE(shapes)) return;
    style_json_root(shapes[shape]);
}

static int name_bar_shown(const int style) {
    if (!(style_options[style] & OPT(namex))) return 0;
    return !(style_options[style] & OPT(names)) || layout_values[layout_names] == 0;
}

static void name_bar_adjust(struct theme_config *theme) {
    theme->grid.current_item_label.offset_x =
        (int16_t) (theme->grid.current_item_label.offset_x + style_number_value(number_namex));
    theme->grid.current_item_label.offset_y =
        (int16_t) (theme->grid.current_item_label.offset_y + style_number_value(number_namey));
    if (style_tweaks_current()->number_set & (1u << number_namewidth)) {
        theme->grid.current_item_label.width =
            (int16_t) (theme->grid.current_item_label.width * style_number_value(number_namewidth) / 100);
    }
    if (style_tweaks_current()->number_set & (1u << number_namebg)) {
        theme->grid.current_item_label.background_alpha = (int16_t) (style_number_value(number_namebg) * 255 / 100);
    }
}

int menu_name_angle(void) {
    return menu_style_active() && name_bar_shown(current_style()) ? style_number_value(number_nameangle) : 0;
}

int menu_style_active(void) {
    return theme_allows_styles && chosen_style > menu_style_theme && strcmp(mux_module, "muxlaunch") == 0;
}

const int *menu_item_order(void) {
    static int order[MENU_ITEM_COUNT];
    int seen[MENU_ITEM_COUNT] = {0};
    int used = 0;

    for (const char *c = theme_allows_styles ? config.settings.themeopt.menu_order : ""; *c && used < MENU_ITEM_COUNT;
         c++) {
        const int item = *c - '0';
        if (item < 0 || item >= MENU_ITEM_COUNT || seen[item]) continue;
        seen[item] = 1;
        order[used++] = item;
    }

    for (int item = 0; item < MENU_ITEM_COUNT; item++) {
        if (!seen[item]) order[used++] = item;
    }

    return order;
}

int menu_style_paged_active(void) {
    return menu_style_active() && chosen_style == menu_style_paged;
}

int menu_style_any_direction(void) {
    if (!menu_style_active() || menu_style_spatial()) return 0;

    return chosen_style == menu_style_clock || theme.grid.row_count == 1 || theme.grid.column_count == 1;
}

int menu_style_blocks(const int vertical) {
    if (!menu_style_active()) return 0;

    const int style = chosen_style;
    if (style == menu_style_list) return !vertical;
    if (style == menu_style_paged) return vertical;

    return 0;
}

int menu_style_spatial(void) {
    if (!menu_style_active()) return 0;

    switch (chosen_style) {
        case menu_style_grid:
        case menu_style_bubbles:
        case menu_style_honeycomb:
        case menu_style_tiles:
        case menu_style_fluent:
        case menu_style_desktop:
        case menu_style_paged:
        case menu_style_mascot:
            return 1;
        default:
            return 0;
    }
}

const char *menu_style_layout_json(void) {
    return menu_style_active() ? style_json() : NULL;
}

int menu_style_index(const char *name) {
    if (!name) return 0;
    while (*name == ' ' || *name == '\t')
        name++;

    for (int i = 1; name[0] && i < (int) (sizeof(style_keys) / sizeof(style_keys[0])); i++) {
        const size_t length = strlen(style_keys[i]);
        if (strncasecmp(style_keys[i], name, length) == 0 && (unsigned char) name[length] <= ' ') return i;
    }
    return 0;
}

int menu_style_named_for(const char *theme_path) {
    static const char *const files[] = {"global", "default", "muxlaunch"};
    const char *dims[] = {"", mux_dim};
    char name[64] = "";

    for (size_t f = 0; f < A_SIZE(files); f++) {
        for (size_t d = 0; d < A_SIZE(dims); d++) {
            char path[MAX_BUFFER_SIZE];
            snprintf(path, sizeof(path), "%s/%sscheme/%s.ini", theme_path, dims[d], files[f]);
            mini_t *ini = mini_try_load(path);
            if (!ini) continue;

            const char *value = mini_get_string(ini, "misc", "MENU_STYLE_NAME", "");
            if (value && *value) snprintf(name, sizeof(name), "%s", value);
            mini_free(ini);
        }
    }

    char *data = theme_json_read(theme_path);
    if (data) {
        const struct json root = json_parse(data);
        const struct json parts[] = {json_object_get(root, "scheme"), theme_json_module(root, "muxlaunch", "scheme")};
        for (size_t i = 0; i < A_SIZE(parts); i++) {
            const struct json value = theme_json_lookup(parts[i], "misc", "MENU_STYLE_NAME");
            if (json_exists(value)) json_string_copy(value, name, sizeof(name));
        }
        free(data);
    }

    return menu_style_index(name);
}

void menu_style_apply(struct theme_config *theme, const int width, const int height) {
    style_json_reset();
    if (strcmp(mux_module, "muxlaunch") == 0) {
        const int list_hidden = theme->list_default.text_alpha == 0 && theme->list_focus.text_alpha == 0;
        theme_allows_styles = theme->misc.menu_style >= 0 ? theme->misc.menu_style > 0 : !list_hidden;
        chosen_style = config.settings.themeopt.menu_style;
        const int named = chosen_style == menu_style_theme ? menu_style_index(theme->misc.menu_style_name) : 0;
        if (named) {
            chosen_style = named;
            theme_allows_styles = 1;
            config.settings.themeopt.menu_style = (int16_t) named;
            write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_style", INT, named);
        }
    }
    if (!menu_style_active()) return;

    const int style = chosen_style;
    card_art = detect_card_art();
    if (card_art) {
        style_solid_cells(theme);
        theme->grid.cell_default.image_recolour = theme->grid.cell_default.text;
        theme->grid.cell_focus.image_recolour = theme->grid.cell_focus.text;
        theme->grid.cell_default.image_recolour_alpha = LV_OPA_COVER;
        theme->grid.cell_focus.image_recolour_alpha = LV_OPA_COVER;
    }
    if (!layout_editing) {
        style_tweaks_load(style);
        layout_load(theme);
    }
    const int position = layout_values[layout_position];
    const int side_style = style == menu_style_banners || style == menu_style_rolodex || style == menu_style_sidebar
                           || style == menu_style_desktop;
    style_layout_set(layout_values, side_style && position == 2, width, height);
    if (style == menu_style_list) {
        theme->grid.row_count = 0;
        theme->grid.column_count = 0;
        theme->misc.navigation_type = nav_standard;
        theme->misc.content.alignment = (int16_t) layout_values[layout_position];
        theme->misc.content.size_to_content = (int16_t) layout_values[layout_fit];
        return;
    }

    if (style < (int) (sizeof(style_apply) / sizeof(style_apply[0])) && style_apply[style]) {
        style_apply[style](theme, width, height);
    }

    const style_tweaks *tweaks = style_tweaks_current();
    if (tweaks->time > 0) theme->grid.animation_time = (int16_t) tweaks->time;
    if (style_json()) {
        char root[64];
        if ((style_options[style] & OPT(text)) && style_number_value(number_text) != 100) {
            snprintf(root, sizeof(root), "\"text_size\":%d,", style_number_value(number_text));
            style_json_root(root);
        }
        if (tweaks->swell >= 0) {
            snprintf(root, sizeof(root), "\"focus_zoom\":%d,", tweaks->swell);
            style_json_root(root);
        }
        if (tweaks->mascot_x || tweaks->mascot_y) {
            snprintf(root, sizeof(root), "\"mascot_nudge\":[%d,%d],", tweaks->mascot_x, tweaks->mascot_y);
            style_json_root(root);
        }
        if (tweaks->bounce_set) {
            snprintf(root, sizeof(root), "\"mascot_bounce\":[%d,%d],", style_bounce_height(), style_bounce_speed());
            style_json_root(root);
        }
        if (tweaks->motion == motion_glide) style_json_root("\"motion\":\"ease\",");
        if (tweaks->motion == motion_swing) style_json_root("\"motion\":\"swing\",");
        if (tweaks->motion == motion_orbit) style_json_root("\"motion\":\"orbit\",");
    }

    if (name_bar_shown(style)) name_bar_adjust(theme);
    if (style_json()) cell_shape(style);

    if (!(style_options[style] & OPT(names)) || style == menu_style_desktop) return;
    if (layout_values[layout_names] == 1) {
        const int cell = theme->grid.cell.width;
        style_show_cell_text(theme);
        theme->glyph.grid = (int16_t) (cell / 2);
        theme->grid.cell.image_padding_top = (int16_t) (cell / 10);
        theme->grid.cell.text_padding_bottom = (int16_t) (cell / 16);
    }
    if (layout_values[layout_names]) style_hide_name_bar(theme);
}
