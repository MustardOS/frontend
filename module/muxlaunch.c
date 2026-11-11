#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/grid_layout.h>
#include <common/ui/nav/style/shared.h>
#include <common/ui/nav/style/tweak.h>
#include <common/ui/nav/mascot.h>
#include "ui/ui_muxlaunch.h"

static mux_dialogue confirm_dlg;

static mux_dialogue tour_dlg;
static int pending_power_action = 0;
static uint32_t arc_hold_started = 0;
static int arc_hold_fired = 0;

static void show_confirm_dialog(const char *title) {
    lv_label_set_text(confirm_dlg.title_label, title);
    dialogue_open(&confirm_dlg, &theme);
}

static void show_tour_offer(void) {
    tour_dlg.active = 1;
    dialogue_open(&tour_dlg, &theme);
}

static void answer_tour_offer(const int accepted) {
    dialogue_dismiss(&tour_dlg);

    if (accepted) {
        orientation_accept();
        orientation_introduce(mux_module, lang.muxlaunch.title, lang.muxlaunch.overview);
        return;
    }

    orientation_decline();
}

#define LAUNCH(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(LAUNCH_ELEMENTS) };
#undef LAUNCH

#define LAUNCH(NAME, UDATA) launch_item_##NAME,
enum { LAUNCH_ELEMENTS };
#undef LAUNCH

static const int *order;
static char *shown_labels[ui_count_dynamic];
static char *shown_glyphs[ui_count_dynamic];
static int moving = -1;
static int layout_edit = 0;
static lv_obj_t *layout_panel = NULL;
static lv_obj_t *layout_heading = NULL;
static lv_obj_t *layout_list = NULL;

#define LAYOUT_ROWS    20
#define LAYOUT_VISIBLE 7

enum {
    row_option,
    row_number,
    row_bounce,
    row_nudge,
    row_cascade,
    row_motion,
    row_time,
    row_swell,
    row_target,
    row_tweak
};

typedef struct {
    int kind;
    int field;
} layout_row;

static lv_obj_t *layout_values_shown[LAYOUT_ROWS];
static lv_obj_t *layout_rows_shown[LAYOUT_ROWS];
static layout_row layout_rows[LAYOUT_ROWS];
static int layout_row_count = 0;
static int layout_selected = 0;
static int layout_page = 0;
static int layout_pages = 1;
static int layout_slots = 0;
static int layout_target = tweak_target_override;
static int layout_row_height = 0;
static lv_timer_t *layout_timer = NULL;
static int layout_pending = 0;
static lv_obj_t *page_dots[2];

static void list_nav_move(int steps, int direction);

static void place_item(void);

static void restyle_list(void);

static void update_nav(void);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define LAUNCH(NAME, UDATA) {UDATA, lang.muxlaunch.help.NAME},
        LAUNCH_ELEMENTS
#undef LAUNCH
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *item_labels[] = {lang.muxlaunch.explore, lang.muxlaunch.collection, lang.muxlaunch.history,
                           lang.muxlaunch.apps,    lang.muxlaunch.info,       lang.muxlaunch.config,
                           lang.muxlaunch.reboot,  lang.muxlaunch.shutdown};

    char *item_labels_short[] = {lang.muxlaunch.abbr.explore, lang.muxlaunch.abbr.collection,
                                 lang.muxlaunch.abbr.history, lang.muxlaunch.abbr.app,
                                 lang.muxlaunch.abbr.info,    lang.muxlaunch.abbr.config,
                                 lang.muxlaunch.abbr.reboot,  lang.muxlaunch.abbr.shutdown};

    static char *glyph_names[] = {"explore", "collection", "history", "apps", "info", "config", "reboot", "shutdown"};

    order = menu_item_order();
    char *shown_short[ui_count_dynamic];
    for (int i = 0; i < ui_count_dynamic; i++) {
        shown_labels[i] = item_labels[order[i]];
        shown_short[i] = item_labels_short[order[i]];
        shown_glyphs[i] = glyph_names[order[i]];
    }

    reset_ui_groups();

    if (theme.grid.enabled) {
        char **names = grid_layout_full_names() ? shown_labels : shown_short;
        const int steps = init_grid_static(ui_count_dynamic, shown_labels, names, shown_glyphs);
        if (!config.settings.themeopt.menu_label || grid_layout_full_names()) {
            lv_obj_add_flag(ui_pnl_grid_current_item, LV_OBJ_FLAG_HIDDEN);
        }
        const int name_angle = menu_name_angle();
        if (name_angle) {
            lv_obj_update_layout(ui_pnl_grid_current_item);
            const lv_coord_t pivot_x = (lv_coord_t) (lv_obj_get_width(ui_pnl_grid_current_item) / 2);
            const lv_coord_t pivot_y = (lv_coord_t) (lv_obj_get_height(ui_pnl_grid_current_item) / 2);
            lv_obj_set_style_transform_pivot_x(ui_pnl_grid_current_item, pivot_x, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_transform_pivot_y(ui_pnl_grid_current_item, pivot_y, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_transform_angle(
                ui_pnl_grid_current_item, (lv_coord_t) (-name_angle * 10), MU_OBJ_MAIN_DEFAULT
            );
        }
        list_nav_move(steps, +1);
    } else {
        for (int i = 0; i < ui_count_dynamic; i++) {
            switch (order[i]) {
#define LAUNCH(NAME, UDATA)                                                                                            \
    case launch_item_##NAME:                                                                                           \
        INIT_STATIC_ITEM(-1, launch, NAME, shown_labels[i], shown_glyphs[i], 0);                                       \
        break;
                LAUNCH_ELEMENTS
#undef LAUNCH
                default:
                    break;
            }
            lv_obj_move_to_index(ui_objects_panel[i], i);
        }

        add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);
        if (menu_style_active()) restyle_list();
        list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
    }
}

#define WIGGLE_MAX 16

static lv_obj_t *wiggle_panel[WIGGLE_MAX];
static int wiggle_angle[WIGGLE_MAX];

static void wiggle_exec(void *var, const int32_t v) {
    for (int i = 0; i < WIGGLE_MAX; i++) {
        if (wiggle_panel[i] != var) continue;
        lv_obj_set_style_transform_angle(var, (lv_coord_t) (wiggle_angle[i] + v), MU_OBJ_MAIN_DEFAULT);
        return;
    }
}

static lv_obj_t *wiggle_container(void) {
    return grid_mode_enabled ? ui_pnl_grid : ui_pnl_content;
}

static int wiggle_item(lv_obj_t *panel, const int index) {
    return grid_mode_enabled ? (int) IFU(lv_obj_get_user_data(panel)) : index;
}

static int wiggle_base(lv_obj_t *panel, const int index) {
    if (!grid_mode_enabled) return 0;

    const grid_layout_entry layout = find_grid_layout(wiggle_item(panel, index), index);
    return layout.found ? layout.angle * 10 : 0;
}

static void wiggle_stop(void) {
    lv_obj_t *container = wiggle_container();
    const int count = (int) lv_obj_get_child_cnt(container);

    for (int i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(container, i);
        if (!lv_anim_del(panel, wiggle_exec)) continue;
        lv_obj_set_style_transform_angle(panel, (lv_coord_t) wiggle_base(panel, i), MU_OBJ_MAIN_DEFAULT);
    }
}

static void wiggle_start(void) {
    if (config.visual.reduce_motion) return;

    lv_obj_t *container = wiggle_container();
    lv_obj_update_layout(container);
    const int count = (int) lv_obj_get_child_cnt(container);

    for (int i = 0; i < count && i < WIGGLE_MAX; i++) {
        lv_obj_t *panel = lv_obj_get_child(container, i);
        wiggle_panel[i] = panel;
        wiggle_angle[i] = wiggle_base(panel, i);
        if (wiggle_item(panel, i) == moving || lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN)) continue;

        const int width = lv_obj_get_width(panel);
        const int swing = LV_CLAMP(5, 1700 / (width > 0 ? width : 1), 25);
        const int side = i % 2 ? 1 : -1;

        lv_obj_set_style_transform_pivot_x(panel, (lv_coord_t) (width / 2), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_pivot_y(panel, (lv_coord_t) (lv_obj_get_height(panel) / 2), MU_OBJ_MAIN_DEFAULT);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, panel);
        lv_anim_set_exec_cb(&anim, wiggle_exec);
        lv_anim_set_values(&anim, -swing * side, swing * side);
        lv_anim_set_time(&anim, 140);
        lv_anim_set_playback_time(&anim, 140);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&anim, (uint32_t) (i * 37 % 140));
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
        lv_anim_start(&anim);
    }
}

static void paged_repaint(lv_event_t *e) {
    LV_UNUSED(e);
    lv_obj_invalidate(ui_screen);
}

static void paged_update(const int animate) {
    if (!menu_style_paged_active() || !ui_pnl_grid) return;

    if (!page_dots[0]) {
        lv_obj_add_event_cb(ui_pnl_grid, paged_repaint, LV_EVENT_SCROLL, NULL);
        lv_obj_add_event_cb(ui_pnl_grid, paged_repaint, LV_EVENT_SCROLL_END, NULL);
    }

    lv_coord_t dots_y = (lv_coord_t) (device.mux.height - theme.footer.height - 30);
    lv_obj_t *first = lv_obj_get_child(ui_pnl_grid, 0);
    if (first) {
        lv_area_t cell;
        lv_obj_update_layout(ui_pnl_grid);
        lv_obj_get_coords(first, &cell);
        dots_y = (lv_coord_t) (cell.y2 + 14);
    }

    const int page = current_item_index / 4;
    lv_obj_scroll_to_x(ui_pnl_grid, (lv_coord_t) (page * device.mux.width), animate ? LV_ANIM_ON : LV_ANIM_OFF);

    for (int i = 0; i < 2; i++) {
        if (!page_dots[i]) {
            page_dots[i] = lv_obj_create(ui_screen);
            lv_obj_remove_style_all(page_dots[i]);
            lv_obj_set_size(page_dots[i], 10, 10);
            lv_obj_set_style_radius(page_dots[i], LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_bg_opa(page_dots[i], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        }
        lv_obj_align(page_dots[i], LV_ALIGN_TOP_MID, (lv_coord_t) (i * 20 - 10), dots_y);
        const int active = i == page;
        lv_obj_set_style_bg_color(page_dots[i], lv_color_hex(theme.grid.cell_focus.background), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(page_dots[i], active ? LV_OPA_COVER : LV_OPA_40, MU_OBJ_MAIN_DEFAULT);
    }
}

static void list_nav_move(const int steps, const int direction) {
    if (moving >= 0) wiggle_stop();

    const int opening = first_open;
    const int restore = first_open && is_carousel_grid_mode();

    if (first_open) {
        first_open = 0;
    } else {
        nav_unsuppress_shake();
        play_sound(snd_navigate);
    }

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_dynamic - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_dynamic - 1 ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        const int passing = menu_style_active() && !is_carousel_grid_mode() && step < steps - 1;
        if (grid_mode_enabled && !restore && !passing) update_grid(direction);
    }

    if (restore) update_grid_items(1);

    if (!grid_mode_enabled) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_dynamic, current_item_index, ui_pnl_content
        );
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    } else {
        lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].name);
    }

    if (moving >= 0) wiggle_start();
    paged_update(!opening);
    if (grid_mode_enabled && !is_carousel_grid_mode()) {
        mascot_follow(ui_screen, lv_group_get_focused(ui_group_panel));
    } else {
        mascot_remove();
    }
    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static int spatial_move(const int dx, const int dy) {
    if (!menu_style_spatial()) return 0;

    const int target = grid_spatial_target(dx, dy);
    if (target > current_item_index) list_nav_next(target - current_item_index);
    if (target >= 0 && target < current_item_index) list_nav_prev(current_item_index - target);

    return 1;
}

static const char *layout_name(const int option) {
    const char *names[] = {
        lang.muxlaunch.layout.size,           lang.muxlaunch.layout.spacing,     lang.muxlaunch.layout.tilt,
        lang.muxlaunch.layout.swell,          lang.muxlaunch.layout.position,    lang.muxlaunch.layout.pile,
        lang.muxlaunch.layout.names,          lang.muxlaunch.layout.large,       lang.muxlaunch.layout.fit,
        lang.muxlaunch.layout.glyph_position, lang.muxlaunch.layout.text_size,   lang.muxlaunch.layout.focus_width,
        lang.muxlaunch.layout.perch,          lang.muxlaunch.layout.slice_text,  lang.muxlaunch.layout.bounce_height,
        lang.muxlaunch.layout.slice_gap,      lang.muxlaunch.layout.slant,       lang.muxlaunch.layout.text_x,
        lang.muxlaunch.layout.text_back,      lang.muxlaunch.layout.text_y,      lang.muxlaunch.layout.text_angle,
        lang.muxlaunch.layout.name_x,         lang.muxlaunch.layout.name_y,      lang.muxlaunch.layout.name_angle,
        lang.muxlaunch.layout.name_back,      lang.muxlaunch.layout.mascot_size, lang.muxlaunch.layout.shape,
        lang.muxlaunch.layout.glyph_scale,    lang.muxlaunch.layout.name_width
    };
    if (option == layout_position && !theme.grid.enabled) return lang.muxlaunch.layout.text_position;
    return names[option];
}

static const char *layout_choice(const int option, const int value) {
    if (option == layout_shape) {
        const char *shapes[] = {
            lang.muxlaunch.layout.standard, lang.muxlaunch.layout.square, lang.muxlaunch.layout.round,
            lang.muxlaunch.layout.hexagon
        };
        return shapes[LV_CLAMP(0, value, 3)];
    }
    const char *choices[][3] = {
        {lang.muxlaunch.layout.smaller, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.larger},
        {lang.muxlaunch.layout.tighter, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.wider},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.more},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.more},
        {lang.muxlaunch.layout.left, lang.muxlaunch.layout.centre, lang.muxlaunch.layout.right},
        {lang.muxlaunch.layout.up, lang.muxlaunch.layout.left, lang.muxlaunch.layout.right},
        {lang.muxlaunch.layout.bar, lang.muxlaunch.layout.under, lang.muxlaunch.layout.hidden},
        {lang.muxlaunch.layout.three, lang.muxlaunch.layout.four, lang.muxlaunch.layout.four},
        {lang.muxlaunch.layout.full, lang.muxlaunch.layout.fit_text, lang.muxlaunch.layout.fit_text},
        {lang.muxlaunch.layout.left, lang.muxlaunch.layout.right, lang.muxlaunch.layout.right},
        {lang.muxlaunch.layout.smaller, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.larger},
        {lang.muxlaunch.layout.standard, lang.muxlaunch.layout.standard, lang.muxlaunch.layout.standard},
        {lang.muxlaunch.layout.between, lang.muxlaunch.layout.above, lang.muxlaunch.layout.below},
        {lang.muxlaunch.layout.hidden, lang.muxlaunch.layout.shown, lang.muxlaunch.layout.shown},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.standard, lang.muxlaunch.layout.round, lang.muxlaunch.layout.hexagon},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
        {lang.muxlaunch.layout.off, lang.muxlaunch.layout.off, lang.muxlaunch.layout.off},
    };
    return choices[option][value];
}

static const char *tweak_name(const int field) {
    const char *names[] = {lang.muxlaunch.layout.move_x, lang.muxlaunch.layout.move_y,   lang.muxlaunch.layout.width,
                           lang.muxlaunch.layout.height, lang.muxlaunch.layout.rotation, lang.muxlaunch.layout.tilt_x,
                           lang.muxlaunch.layout.tilt_y, lang.muxlaunch.layout.depth,    lang.muxlaunch.layout.opacity,
                           lang.muxlaunch.layout.glyph};
    return names[field];
}

static const char *cascade_name(const int field) {
    const char *names[] = {
        lang.muxlaunch.layout.cascade_size, lang.muxlaunch.layout.cascade_spacing, lang.muxlaunch.layout.cascade_angle,
        lang.muxlaunch.layout.cascade_tilt, lang.muxlaunch.layout.cascade_fade
    };
    return names[field];
}

static const char *motion_name(const int motion) {
    const char *names[] = {
        lang.muxlaunch.layout.standard, lang.muxlaunch.layout.glide, lang.muxlaunch.layout.swing,
        lang.muxlaunch.layout.orbit
    };
    return names[motion];
}

typedef struct {
    int min, max, step, start;
} edit_range;

typedef struct {
    int option, min, max, step;
    const char *unit;
} edit_number;

static const edit_number numbers[NUMBER_COUNT] = {
    [number_size] = {layout_size, 20, 300, 1, "%d%%"},
    [number_spacing] = {layout_spacing, 20, 400, 1, "%d%%"},
    [number_tilt] = {layout_tilt, -500, 500, 1, "%d%%"},
    [number_swell] = {layout_swell, 0, 500, 1, "%d%%"},
    [number_text] = {layout_text, 25, 400, 1, "%d%%"},
    [number_focus] = {layout_focus, 10, 95, 1, "%d%%"},
    [number_gap] = {layout_gap, 0, 200, 1, "%dpx"},
    [number_slant] = {layout_slant, -60, 60, 1, "%d\u00B0"},
    [number_textbg] = {layout_textbg, 0, 100, 1, "%d%%"},
    [number_textx] = {layout_textx, -960, 960, 1, "%+dpx"},
    [number_texty] = {layout_texty, 0, 1080, 1, "%dpx"},
    [number_textangle] = {layout_textangle, -180, 180, 1, "%d\u00B0"},
    [number_namex] = {layout_namex, -960, 960, 1, "%+dpx"},
    [number_namey] = {layout_namey, -1080, 1080, 1, "%+dpx"},
    [number_nameangle] = {layout_nameangle, -180, 180, 1, "%d\u00B0"},
    [number_namebg] = {layout_namebg, 0, 100, 1, "%d%%"},
    [number_mascotscale] = {layout_mascotscale, 10, 500, 1, "%d%%"},
    [number_glyphscale] = {layout_glyphscale, 10, 400, 1, "%d%%"},
    [number_namewidth] = {layout_namewidth, 10, 300, 1, "%d%%"},
};

static int option_number(const int option) {
    for (int id = 0; id < NUMBER_COUNT; id++) {
        if (numbers[id].option == option) return id;
    }
    return -1;
}

static const char *target_name(const int target) {
    const char *names[] = {
        lang.muxlaunch.layout.save_override, lang.muxlaunch.layout.save_package, lang.muxlaunch.layout.save_global
    };
    return names[target];
}

static edit_range tweak_range(const int field) {
    switch (field) {
        case tweak_dx:
        case tweak_dy:
        case tweak_dw:
        case tweak_dh:
            return (edit_range) {-1000, 1000, 2, 0};
        case tweak_angle:
            return (edit_range) {-360, 360, 1, 0};
        case tweak_tilt_x:
        case tweak_tilt_y:
            return (edit_range) {-89, 89, 1, 0};
        case tweak_depth:
            return (edit_range) {-20, 20, 1, 0};
        case tweak_opacity:
            return (edit_range) {0, 255, 3, 255};
        default:
            return (edit_range) {4, 512, 1, theme.grid.cell.width / 2};
    }
}

static const edit_range cascade_ranges[CASCADE_FIELD_COUNT] = {
    {-60, 60, 1, 0}, {-500, 500, 2, 0}, {-90, 90, 1, 0}, {-89, 89, 1, 0}, {0, 100, 1, 0},
};

static int edit_step(const int value, const int direction, const edit_range range) {
    return LV_CLAMP(range.min, value + direction * range.step, range.max);
}

static int edit_first_position(void) {
    return layout_slots && style_json() ? 2 : 1;
}

static int edit_slot_offset(void) {
    const int page = layout_page - edit_first_position();
    return page % 2 ? -(page + 1) / 2 : page / 2;
}

static style_tweak *edit_tweak(void) {
    style_tweaks *tweaks = style_tweaks_current();
    if (layout_slots) return &tweaks->slots[layout_slots / 2 + edit_slot_offset()];
    return &tweaks->items[order[layout_page - edit_first_position()]];
}

static void edit_add(const int kind, const int field) {
    if (layout_row_count < LAYOUT_ROWS) layout_rows[layout_row_count++] = (layout_row) {kind, field};
}

static void edit_rows_build(void) {
    layout_row_count = 0;
    if (layout_page && layout_page < edit_first_position()) {
        for (int field = 0; field < CASCADE_FIELD_COUNT; field++)
            edit_add(row_cascade, field);
        edit_add(row_target, 0);
        return;
    }

    if (layout_page) {
        for (int field = 0; field < TWEAK_FIELD_COUNT; field++) {
            if (field != tweak_depth || !layout_slots) edit_add(row_tweak, field);
        }
        edit_add(row_target, 0);
        return;
    }

    static const int shown[] = {
        layout_size,     layout_spacing, layout_shape,     layout_glyphscale, layout_tilt,        layout_swell,
        layout_position, layout_glyph,   layout_perch,     layout_pile,       layout_names,       layout_slicetext,
        layout_textx,    layout_texty,   layout_textangle, layout_textbg,     layout_text,        layout_large,
        layout_fit,      layout_focus,   layout_gap,       layout_slant,      layout_mascotscale, layout_bounce,
        layout_namex,    layout_namey,   layout_nameangle, layout_namebg,     layout_namewidth
    };
    const int options = menu_layout_options();
    for (size_t i = 0; i < A_SIZE(shown); i++) {
        if (!(options & (1 << shown[i]))) continue;
        const int text_row = shown[i] == layout_textx || shown[i] == layout_texty || shown[i] == layout_textangle
                             || shown[i] == layout_textbg;
        if (text_row && !menu_layout_value(layout_slicetext)) continue;
        const int name_row = shown[i] == layout_namex || shown[i] == layout_namey || shown[i] == layout_nameangle
                             || shown[i] == layout_namebg || shown[i] == layout_namewidth;
        const int names_off = (options & (1 << layout_names)) && menu_layout_value(layout_names) != 0;
        if (name_row && (names_off || !config.settings.themeopt.menu_label)) continue;
        if (shown[i] == layout_bounce) {
            edit_add(row_bounce, 0);
            edit_add(row_bounce, 1);
            edit_add(row_nudge, 0);
            edit_add(row_nudge, 1);
            continue;
        }
        const int number = option_number(shown[i]);
        edit_add(number >= 0 ? row_number : row_option, number >= 0 ? number : shown[i]);
    }
    if (style_json()) {
        if (layout_slots) edit_add(row_motion, 0);
        edit_add(row_time, 0);
        edit_add(row_swell, 0);
    }
    edit_add(row_target, 0);
}

static const char *edit_row_name(const layout_row *row) {
    switch (row->kind) {
        case row_option:
            return layout_name(row->field);
        case row_number:
            return layout_name(numbers[row->field].option);
        case row_bounce:
            return row->field ? lang.muxlaunch.layout.bounce_speed : lang.muxlaunch.layout.bounce_height;
        case row_nudge:
            return row->field ? lang.muxlaunch.layout.mascot_y : lang.muxlaunch.layout.mascot_x;
        case row_cascade:
            return cascade_name(row->field);
        case row_motion:
            return lang.muxlaunch.layout.movement;
        case row_time:
            return lang.muxlaunch.layout.speed;
        case row_swell:
            return lang.muxlaunch.layout.zoom;
        case row_target:
            return lang.muxlaunch.layout.save_to;
        default:
            return tweak_name(row->field);
    }
}

static void edit_tweak_value(char *out, const size_t size, const int field) {
    const style_tweak *tweak = edit_tweak();
    if (!(tweak->mask & (1u << field))) {
        snprintf(out, size, "%s", lang.muxlaunch.layout.standard);
        return;
    }

    const int value = tweak->value[field];
    switch (field) {
        case tweak_dx:
        case tweak_dw:
            snprintf(out, size, "%+dpx", value * device.mux.width / 1000);
            break;
        case tweak_dy:
        case tweak_dh:
            snprintf(out, size, "%+dpx", value * device.mux.height / 1000);
            break;
        case tweak_opacity:
            snprintf(out, size, "%d%%", value * 100 / 255);
            break;
        case tweak_glyph:
            snprintf(out, size, "%dpx", value);
            break;
        default:
            snprintf(out, size, "%+d", value);
            break;
    }
}

static void edit_row_value(char *out, const size_t size, const layout_row *row) {
    const style_tweaks *tweaks = style_tweaks_current();
    switch (row->kind) {
        case row_option:
            snprintf(out, size, "%s", layout_choice(row->field, menu_layout_value(row->field)));
            break;
        case row_number:
            snprintf(out, size, numbers[row->field].unit, style_number_value(row->field));
            break;
        case row_nudge:
            snprintf(out, size, "%+dpx", row->field ? tweaks->mascot_y : tweaks->mascot_x);
            break;
        case row_bounce:
            if (row->field) {
                snprintf(out, size, "%dms", style_bounce_speed());
            } else if (style_bounce_height()) {
                snprintf(out, size, "%d%%", style_bounce_height());
            } else {
                snprintf(out, size, "%s", lang.muxlaunch.layout.off);
            }
            break;
        case row_cascade: {
            const int value = tweaks->cascade[row->field];
            if (!value) {
                snprintf(out, size, "%s", lang.muxlaunch.layout.off);
            } else if (row->field == cascade_spacing) {
                snprintf(out, size, "%+dpx", value * device.mux.width / 1000);
            } else if (row->field == cascade_size || row->field == cascade_fade) {
                snprintf(out, size, "%+d%%", value);
            } else {
                snprintf(out, size, "%+d", value);
            }
            break;
        }
        case row_motion:
            snprintf(out, size, "%s", motion_name(tweaks->motion));
            break;
        case row_time:
            if (tweaks->time > 0) {
                snprintf(out, size, "%dms", tweaks->time);
            } else {
                snprintf(out, size, "%s", lang.muxlaunch.layout.standard);
            }
            break;
        case row_swell:
            if (tweaks->swell >= 0) {
                snprintf(out, size, "%d%%", tweaks->swell);
            } else {
                snprintf(out, size, "%s", lang.muxlaunch.layout.standard);
            }
            break;
        case row_target:
            snprintf(out, size, "%s", target_name(layout_target));
            break;
        default:
            edit_tweak_value(out, size, row->field);
            break;
    }
}

static void layout_title(void) {
    char title[MAX_BUFFER_SIZE];
    if (!layout_page) {
        snprintf(title, sizeof(title), "%s", lang.muxlaunch.layout.page);
    } else if (layout_page < edit_first_position()) {
        snprintf(title, sizeof(title), "%s", lang.muxlaunch.layout.outer);
    } else if (layout_slots && !edit_slot_offset()) {
        snprintf(title, sizeof(title), "%s", lang.muxlaunch.layout.focused);
    } else if (layout_slots) {
        snprintf(title, sizeof(title), lang.muxlaunch.layout.slot, edit_slot_offset());
    } else {
        snprintf(title, sizeof(title), "%s", shown_labels[layout_page - edit_first_position()]);
    }

    char shown[MAX_BUFFER_SIZE];
    snprintf(shown, sizeof(shown), "%s  %d / %d", title, layout_page + 1, layout_pages);
    lv_label_set_text(layout_heading, shown);
}

static void layout_paint(void) {
    for (int i = 0; i < layout_row_count; i++) {
        const int selected = i == layout_selected;
        char value[MAX_BUFFER_SIZE];
        edit_row_value(value, sizeof(value), &layout_rows[i]);
        lv_label_set_text(layout_values_shown[i], value);

        lv_obj_set_style_bg_opa(
            layout_rows_shown[i], selected ? (lv_opa_t) theme.dialogue.selection_alpha : LV_OPA_TRANSP,
            MU_OBJ_MAIN_DEFAULT
        );
        const lv_color_t text = lv_color_hex(selected ? theme.dialogue.option : theme.dialogue.content);
        lv_obj_t *labels[] = {lv_obj_get_child(layout_rows_shown[i], 0), layout_values_shown[i]};
        for (size_t j = 0; j < A_SIZE(labels); j++) {
            lv_obj_set_style_text_color(labels[j], text, MU_OBJ_MAIN_DEFAULT);
            if (selected) {
                lv_obj_add_state(labels[j], LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(labels[j], LV_STATE_CHECKED);
            }
        }
    }

    const int top = LV_CLAMP(0, layout_selected - LAYOUT_VISIBLE / 2, LV_MAX(0, layout_row_count - LAYOUT_VISIBLE));
    lv_obj_scroll_to_y(layout_list, (lv_coord_t) (top * layout_row_height), LV_ANIM_OFF);
}

static void layout_fill(void) {
    edit_rows_build();
    layout_selected = 0;
    lv_obj_clean(layout_list);

    const lv_font_t *font = lv_obj_get_style_text_font(layout_panel, LV_PART_MAIN);
    const int row_height = lv_font_get_line_height(font) + 10;
    layout_row_height = row_height;
    const int shown = layout_row_count < LAYOUT_VISIBLE ? layout_row_count : LAYOUT_VISIBLE;
    lv_obj_set_height(layout_list, (lv_coord_t) (shown * row_height));

    for (int i = 0; i < layout_row_count; i++) {
        lv_obj_t *row = lv_obj_create(layout_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), (lv_coord_t) (row_height - 2));
        lv_obj_set_style_pad_hor(row, 10, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_radius(row, theme.dialogue.radius.selected, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(theme.dialogue.selection), MU_OBJ_MAIN_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, edit_row_name(&layout_rows[i]));
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        layout_values_shown[i] = lv_label_create(row);
        lv_obj_align(layout_values_shown[i], LV_ALIGN_RIGHT_MID, 0, 0);
        layout_rows_shown[i] = row;
    }

    layout_title();
    layout_paint();
}

static void layout_open(void) {
    menu_layout_edit(1);
    layout_slots = style_slots_written();
    layout_pages = (layout_slots ? layout_slots : style_items_written()) + edit_first_position();
    layout_target = style_tweaks_source();
    layout_page = 0;

    if (!menu_layout_options() && layout_pages == 1) {
        toast_message(lang.muxlaunch.layout.none, tst_wait_l);
        return;
    }

    layout_panel = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(layout_panel);
    lv_obj_set_size(layout_panel, (lv_coord_t) (device.mux.width * 0.8), LV_SIZE_CONTENT);
    lv_obj_align(layout_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(layout_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(layout_panel, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(layout_panel, 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(layout_panel, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(layout_panel, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(layout_panel, (lv_opa_t) theme.dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(layout_panel, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(layout_panel, (lv_opa_t) theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(layout_panel, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(layout_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_shadow_zone_register(
        layout_panel, lv_color_hex(theme.dialogue.shadow_colour), (lv_opa_t) theme.dialogue.shadow_alpha,
        (int8_t) theme.dialogue.shadow_x_offset, (int8_t) theme.dialogue.shadow_y_offset,
        lv_color_hex(theme.dialogue.shadow_colour_focus), (lv_opa_t) theme.dialogue.shadow_alpha_focus,
        (int8_t) theme.dialogue.shadow_x_offset_focus, (int8_t) theme.dialogue.shadow_y_offset_focus
    );

    lv_obj_t *header = lv_obj_create(layout_panel);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    const char *shoulders[] = {"l1", NULL, "r1"};
    for (size_t i = 0; i < A_SIZE(shoulders); i++) {
        if (!shoulders[i]) {
            layout_heading = lv_label_create(header);
            lv_obj_set_flex_grow(layout_heading, 1);
            lv_obj_set_style_text_align(layout_heading, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_text_color(layout_heading, lv_color_hex(theme.dialogue.title), MU_OBJ_MAIN_DEFAULT);
            continue;
        }

        lv_obj_t *glyph = create_footer_glyph(header, &theme, shoulders[i], theme.nav.lr, 0);
        lv_obj_set_style_pad_all(glyph, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_hor(glyph, 4, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_img_recolor(glyph, lv_color_hex(theme.dialogue.title), MU_OBJ_MAIN_DEFAULT);
        if (layout_pages < 2) lv_obj_add_flag(glyph, LV_OBJ_FLAG_HIDDEN);
    }

    layout_list = lv_obj_create(layout_panel);
    lv_obj_remove_style_all(layout_list);
    lv_obj_set_width(layout_list, LV_PCT(100));
    lv_obj_set_flex_flow(layout_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(layout_list, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_scrollbar_mode(layout_list, LV_SCROLLBAR_MODE_OFF);

    layout_fill();
}

static int layout_shown(void) {
    return layout_edit && layout_panel && !lv_obj_has_flag(layout_panel, LV_OBJ_FLAG_HIDDEN);
}

static void restyle_list_item(lv_obj_t *panel, lv_obj_t *label, lv_obj_t *glyph, const char *text) {
    static const lv_text_align_t aligns[] = {LV_TEXT_ALIGN_LEFT, LV_TEXT_ALIGN_CENTER, LV_TEXT_ALIGN_RIGHT};

    lv_obj_remove_local_style_prop(panel, LV_STYLE_WIDTH, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(label, LV_STYLE_WIDTH, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(label, LV_STYLE_PAD_RIGHT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(label, LV_STYLE_TEXT_ALIGN, MU_OBJ_MAIN_DEFAULT);

    lv_obj_remove_local_style_prop(label, LV_STYLE_PAD_LEFT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(glyph, LV_STYLE_ALIGN, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(glyph, LV_STYLE_X, MU_OBJ_MAIN_DEFAULT);

    const int alignment = LV_CLAMP(0, theme.misc.content.alignment, 2);
    const int fit = theme.misc.content.size_to_content;
    if (!fit && alignment) lv_obj_set_style_text_align(label, aligns[alignment], MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(label, text);
    apply_size_to_content(&theme, ui_pnl_content, label, glyph, text);

    const int edge = LV_MAX(theme.font.list_pad_right, 12);
    if (menu_layout_value(layout_glyph)) {
        lv_obj_set_style_align(glyph, LV_ALIGN_RIGHT_MID, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_x(glyph, (lv_coord_t) -theme.list_default.glyph_padding_left, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_left(label, (lv_coord_t) (fit ? theme.font.list_pad_right : edge), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(label, (lv_coord_t) theme.font.list_pad_left, MU_OBJ_MAIN_DEFAULT);
    } else if (!fit && alignment == 2) {
        lv_obj_set_style_pad_right(label, (lv_coord_t) edge, MU_OBJ_MAIN_DEFAULT);
    }
}

static void restyle_list(void) {
    apply_content_alignment(&theme, ui_pnl_content);
    if (theme.misc.content.alignment == 2 && theme.misc.content.size_to_content) {
        lv_obj_set_style_pad_right(
            ui_pnl_content, (lv_coord_t) LV_MAX(theme.misc.content.padding_left, 10), MU_OBJ_MAIN_DEFAULT
        );
    }
#define LAUNCH(NAME, UDATA)                                                                                            \
    restyle_list_item(ui_pnl_##NAME##_launch, ui_lbl_##NAME##_launch, ui_ico_##NAME##_launch, lang.muxlaunch.NAME);
    LAUNCH_ELEMENTS
#undef LAUNCH
}

static void layout_relayout(void) {
    const int focused = current_item_index;
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, shown_glyphs[focused]);
    load_theme(&theme, &config, &device);

    if (!theme.grid.enabled) {
        restyle_list();
        return;
    }

    lv_obj_clean(ui_pnl_grid);
    free_items(&items, &item_count);
    current_item_index = 0;
    first_open = 1;
    init_navigation_group();

    if (layout_panel) lv_obj_move_foreground(layout_panel);
}

static void layout_focus_item(void) {
    if (layout_slots || !layout_page) return;

    const int target = layout_page - edit_first_position();
    if (target > current_item_index) list_nav_next(target - current_item_index);
    if (target < current_item_index) list_nav_prev(current_item_index - target);
}

static void layout_settle(lv_timer_t *timer);

static void layout_apply(const int throttle) {
    layout_pending = 0;
    layout_relayout();
    layout_focus_item();
    if (!throttle) return;

    layout_timer = lv_timer_create(layout_settle, 120, NULL);
    lv_timer_set_repeat_count(layout_timer, 1);
}

static void layout_settle(lv_timer_t *timer) {
    LV_UNUSED(timer);
    layout_timer = NULL;
    if (layout_pending) layout_apply(1);
}

static void layout_queue(void) {
    if (layout_timer) {
        layout_pending = 1;
        return;
    }

    layout_apply(1);
}

static void layout_flush(void) {
    if (!layout_timer) return;

    lv_timer_del(layout_timer);
    layout_timer = NULL;
    if (layout_pending) layout_apply(0);
}

static void layout_move(const int direction) {
    if (!layout_row_count) return;
    layout_selected = (layout_selected + direction + layout_row_count) % layout_row_count;
    play_sound(snd_navigate);
    layout_paint();
}

static void layout_turn(const int direction) {
    if (!layout_edit || !layout_panel || layout_pages < 2) return;

    layout_page = (layout_page + direction + layout_pages) % layout_pages;
    play_sound(snd_navigate);
    layout_fill();
    layout_focus_item();
}

static void layout_refill(const int kind, const int field) {
    layout_fill();
    for (int i = 0; i < layout_row_count; i++) {
        if (layout_rows[i].kind == kind && layout_rows[i].field == field) layout_selected = i;
    }
}

static uint32_t change_tick = 0;
static int change_row = -1;
static int change_streak = 0;

static int layout_accel(void) {
    const int same = change_row == layout_selected && lv_tick_elaps(change_tick) < 300;
    change_streak = same ? change_streak + 1 : 0;
    change_row = layout_selected;
    change_tick = lv_tick_get();
    return change_streak < 6 ? 1 : change_streak < 16 ? 4 : change_streak < 30 ? 10 : 25;
}

static void layout_change(const int direction) {
    if (!layout_row_count) return;

    const int fast = direction * layout_accel();
    const layout_row *row = &layout_rows[layout_selected];
    style_tweaks *tweaks = style_tweaks_current();
    switch (row->kind) {
        case row_option:
            menu_layout_step(row->field, direction);
            if (row->field == layout_slicetext || row->field == layout_names) layout_refill(row_option, row->field);
            break;
        case row_number: {
            const edit_number *number = &numbers[row->field];
            const int value = style_number_value(row->field) + fast * number->step;
            tweaks->number[row->field] = LV_CLAMP(number->min, value, number->max);
            if (row->field == number_textx) {
                tweaks->number[row->field] = LV_CLAMP(-device.mux.width / 2, value, device.mux.width / 2);
            } else if (row->field == number_texty) {
                tweaks->number[row->field] = LV_CLAMP(0, value, device.mux.height);
            }
            tweaks->number_set |= 1u << row->field;
            break;
        }
        case row_nudge: {
            int *nudge = row->field ? &tweaks->mascot_y : &tweaks->mascot_x;
            *nudge = LV_CLAMP(-300, *nudge + fast, 300);
            break;
        }
        case row_bounce:
            if (row->field) {
                tweaks->bounce_speed = LV_CLAMP(100, style_bounce_speed() + fast * 10, 3000);
                tweaks->bounce_height = style_bounce_height();
            } else {
                tweaks->bounce_height = LV_CLAMP(0, style_bounce_height() + fast, 100);
                tweaks->bounce_speed = style_bounce_speed();
            }
            tweaks->bounce_set = 3;
            break;
        case row_cascade:
            tweaks->cascade[row->field] = edit_step(tweaks->cascade[row->field], fast, cascade_ranges[row->field]);
            break;
        case row_motion:
            tweaks->motion = (tweaks->motion + direction + MOTION_COUNT) % MOTION_COUNT;
            break;
        case row_time:
            tweaks->time = tweaks->time + fast * 10 < 50 ? 0 : LV_CLAMP(50, tweaks->time + fast * 10, 3000);
            if (!tweaks->time && direction > 0) tweaks->time = 50;
            break;
        case row_swell:
            tweaks->swell = tweaks->swell + fast < 100 ? -1 : LV_CLAMP(100, tweaks->swell + fast, 250);
            if (tweaks->swell < 0 && direction > 0) tweaks->swell = 100;
            break;
        case row_target:
            layout_target = (layout_target + direction + TWEAK_TARGET_COUNT) % TWEAK_TARGET_COUNT;
            play_sound(snd_navigate);
            layout_paint();
            return;
        default: {
            style_tweak *tweak = edit_tweak();
            const edit_range range = tweak_range(row->field);
            style_tweak_set(tweak, row->field, edit_step(style_tweak_get(tweak, row->field, range.start), fast, range));
            break;
        }
    }

    play_sound(snd_navigate);
    layout_paint();
    layout_queue();
}

static style_tweak *package_tweak(void) {
    const style_tweaks *package = style_tweaks_package();
    if (!package) return NULL;

    style_tweaks *tweaks = style_tweaks_current();
    const style_tweak *current = edit_tweak();
    const ptrdiff_t offset = (const char *) current - (const char *) tweaks;
    return (style_tweak *) ((const char *) package + offset);
}

static void layout_reset(void) {
    if (!layout_shown() || !layout_row_count) return;

    const layout_row *row = &layout_rows[layout_selected];
    style_tweaks *tweaks = style_tweaks_current();
    const style_tweaks *package = style_tweaks_package();
    style_tweaks base;
    if (package) {
        base = *package;
    } else {
        memset(&base, 0, sizeof(base));
        base.swell = -1;
    }

    switch (row->kind) {
        case row_option:
            menu_layout_default(row->field);
            if (row->field == layout_slicetext || row->field == layout_names) layout_refill(row_option, row->field);
            break;
        case row_number: {
            const unsigned bit = 1u << row->field;
            menu_layout_default(numbers[row->field].option);
            tweaks->number[row->field] = base.number[row->field];
            tweaks->number_set = (tweaks->number_set & ~bit) | (base.number_set & bit);
            break;
        }
        case row_nudge:
            if (row->field) {
                tweaks->mascot_y = base.mascot_y;
            } else {
                tweaks->mascot_x = base.mascot_x;
            }
            break;
        case row_bounce:
            tweaks->bounce_height = base.bounce_height;
            tweaks->bounce_speed = base.bounce_speed;
            tweaks->bounce_set = base.bounce_set;
            break;
        case row_target:
            layout_target = tweak_target_override;
            break;
        case row_cascade:
            tweaks->cascade[row->field] = base.cascade[row->field];
            break;
        case row_motion:
            tweaks->motion = base.motion;
            break;
        case row_time:
            tweaks->time = base.time;
            break;
        case row_swell:
            tweaks->swell = base.swell;
            break;
        case row_tweak: {
            const style_tweak *packaged = package_tweak();
            if (packaged && packaged->mask & (1u << row->field)) {
                style_tweak_set(edit_tweak(), row->field, packaged->value[row->field]);
            } else {
                style_tweak_unset(edit_tweak(), row->field);
            }
            break;
        }
        default:
            return;
    }

    play_sound(snd_back);
    layout_paint();
    if (row->kind != row_target) layout_queue();
}

static void layout_close(void) {
    layout_flush();
    if (!layout_panel) {
        play_sound(snd_back);
        mux_input_stop();
        return;
    }

    const int saved = menu_layout_save(layout_target);
    if (!style_tweaks_save(menu_style_current(), layout_target) || !saved) {
        toast_message(lang.generic.save_fail, tst_wait_m);
    }

    play_sound(snd_back);
    mux_input_stop();
}

static void handle_y(void) {
    if (!layout_edit || !layout_panel) return;

    layout_flush();
    if (lv_obj_has_flag(layout_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(layout_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(layout_panel, LV_OBJ_FLAG_HIDDEN);
    }
    update_nav();
}

static void handle_l1(void) {
    layout_turn(-1);
}

static void handle_r1(void) {
    layout_turn(1);
}

static void handle_start(void) {
    if (!layout_edit || !layout_panel) return;

    layout_flush();
    play_sound(snd_back);
    mux_input_stop();
}

static void handle_a(void) {
    if (layout_edit) return;

    if (dialogue_active(&tour_dlg)) {
        play_sound(snd_confirm);
        answer_tour_offer(tour_dlg.selected == mux_confirm_yep);
        return;
    }

    if (msgbox_active || hold_call) return;

    if (moving >= 0) {
        place_item();
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) confirm_dlg.selected;
        dialogue_dismiss(&confirm_dlg);

        if (opt == mux_confirm_yep) {
            if (pending_power_action == 1) {
                toast_message(lang.generic.rebooting, tst_wait_f);
                load_mux("reboot");
            } else {
                toast_message(lang.generic.shutting_down, tst_wait_f);
                load_mux("shutdown");
            }

            mux_input_stop();
        }

        return;
    }

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_general,
        menu_content,
        menu_reboot,
        menu_shutdown,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action menu_action;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"explore", &kiosk.launch.explore, menu_content},
        {"collection", &kiosk.launch.collection, menu_content},
        {"history", &kiosk.launch.history, menu_content},
        {"app", &kiosk.launch.application, menu_general},
        {"info", &kiosk.launch.information, menu_general},
        {"config", &kiosk.launch.configuration, menu_general},
        {"reboot", &kiosk_pass, menu_reboot},
        {"shutdown", &kiosk_pass, menu_shutdown},
    };

    if ((unsigned) current_item_index >= ui_count_dynamic) return;
    const menu_entry *entry = &entries[order[current_item_index]];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    switch (entry->menu_action) {
        case menu_general:
            play_sound(snd_confirm);
            break;
        case menu_content:
            invalidate_catalogue_cache();
            play_sound(snd_confirm);
            break;
        case menu_reboot:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 1;
                show_confirm_dialog(lang.muxlaunch.confirm_reboot);
                return;
            }
            toast_message(lang.generic.rebooting, tst_wait_f);
            break;
        case menu_shutdown:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 2;
                show_confirm_dialog(lang.muxlaunch.confirm_shutdown);
                return;
            }
            toast_message(lang.generic.shutting_down, tst_wait_f);
            break;
        default:
            return;
    }

    load_mux(entry->mux_name);

    mux_input_stop();
}

static void handle_b(void) {
    if (layout_edit) {
        layout_close();
        return;
    }

    if (dialogue_active(&tour_dlg)) return;
    if (hold_call) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_mark_cancelled(&confirm_dlg);
        dialogue_dismiss(&confirm_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (moving >= 0) {
        wiggle_stop();
        moving = -1;
        update_nav();
        play_sound(snd_back);
        toast_message(lang.muxlaunch.move_cancel, tst_wait_s);
        return;
    }

    if (current_item_index != 0) list_nav_move(current_item_index, -1);
}

static int move_blocked(void) {
    if (layout_edit || dialogue_active(&tour_dlg) || dialogue_active(&confirm_dlg) || msgbox_active || hold_call) {
        return 1;
    }
    if (!is_ksk(kiosk.config.customisation)) return 0;

    kiosk_denied();
    return 1;
}

static void place_item(void) {
    wiggle_stop();

    if (moving == current_item_index) {
        moving = -1;
        update_nav();
        play_sound(snd_back);
        toast_message(lang.muxlaunch.move_cancel, tst_wait_s);
        return;
    }

    char saved[ui_count_dynamic + 1];
    for (int i = 0; i < ui_count_dynamic; i++) {
        const int at = i == moving ? current_item_index : i == current_item_index ? moving : i;
        saved[i] = (char) ('0' + order[at]);
    }
    saved[ui_count_dynamic] = '\0';

    play_sound(snd_confirm);
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_order", CHAR, saved);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, shown_glyphs[moving]);
    refresh_config = 1;

    load_mux("launcher");
    mux_input_stop();
}

static void handle_y_hold(void) {
    if (moving >= 0 || move_blocked()) return;

    moving = current_item_index;
    play_sound(snd_confirm);
    wiggle_start();
    update_nav();

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxlaunch.move_pick, shown_labels[moving]);
    toast_message(message, tst_wait_l);
}

static void handle_x_press(void) {
    if (layout_edit) {
        layout_reset();
        return;
    }

    if (dialogue_active(&tour_dlg)) return;

    orientation_handle_skip();
}

static void handle_x(void) {
    if (layout_edit || dialogue_active(&tour_dlg)) return;

    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call) return;

    if (current_item_index == 5) {
        play_sound(snd_confirm);

        load_mux("kiosk");
        mux_input_stop();
    }
}

static int arc_launch_allowed(void) {
    return current_item_index == 3 && !msgbox_active && !dialogue_active(&tour_dlg) && !dialogue_active(&confirm_dlg)
           && !hold_call;
}

static void handle_arc_press(void) {
    arc_hold_started = arc_launch_allowed() ? mux_input_tick() : 0;
    arc_hold_fired = 0;
}

static void handle_arc_hold(void) {
    if (!arc_hold_started || arc_hold_fired || !arc_launch_allowed()) return;
    if (mux_input_tick() - arc_hold_started < 1000) return;

    arc_hold_fired = 1;
    play_sound(snd_confirm);
    load_mux("arc");
    mux_input_stop();
}

static void handle_arc_release(void) {
    arc_hold_started = 0;
    arc_hold_fired = 0;
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call || dialogue_active(&confirm_dlg)) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    if (layout_shown()) {
        layout_move(-1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (menu_style_blocks(1)) return;
    set_nav_input_dir(nav_dir_up);
    if (spatial_move(0, -1)) return;
    if (menu_style_any_direction()) {
        list_nav_prev(1);
        return;
    }

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_up_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_up_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_next(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_next(misc_steps);
    } else if (theme.misc.navigation_type == nav_split_rows_full && !current_item_index) {
        list_nav_next(2);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 3) {
        list_nav_next(4);
    } else {
        list_nav_prev(1);
    }
}

static void handle_down(void) {
    if (layout_shown()) {
        layout_move(1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (menu_style_blocks(1)) return;
    set_nav_input_dir(nav_dir_down);
    if (spatial_move(0, 1)) return;
    if (menu_style_any_direction()) {
        list_nav_next(1);
        return;
    }

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_down_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_down_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_prev(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_prev(misc_steps);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 7) {
        list_nav_prev(4);
    } else {
        list_nav_next(1);
    }
}

static void handle_up_hold(void) { // prev
    if (layout_shown()) {
        layout_move(-1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad_hold(&tour_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad_hold(&confirm_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_up_ok();
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_up_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index > 0;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index > 0 && current_item_index <= 2)
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index > 3) || is_carousel_grid_mode()
        || menu_style_spatial() || menu_style_any_direction()) {
        handle_up();
    }
}

static void handle_down_hold(void) { // next
    if (layout_shown()) {
        layout_move(1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad_hold(&tour_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad_hold(&confirm_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_down_ok(ui_count_dynamic);
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_down_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index < ui_count_dynamic - 1;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index < ui_count_dynamic - 1
            && current_item_index > 2)
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index < 2) || is_carousel_grid_mode()
        || menu_style_spatial() || menu_style_any_direction()) {
        handle_down();
    }
}

static void handle_left(void) {
    if (layout_shown()) {
        layout_change(-1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, -1, swap_axis);
        return;
    }

    if (menu_style_blocks(0)) return;
    set_nav_input_dir(nav_dir_left);
    if (spatial_move(-1, 0)) return;
    if ((is_carousel_grid_mode() && theme.grid.row_count == 1) || menu_style_any_direction()) {
        list_nav_prev(1);
        return;
    }

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_prev(grid_row_left_steps());
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_prev(ui_count_dynamic / 2);
    }

    if (is_split_row_misc_nav()) {
        switch (current_item_index) {
            case 3:
            case 4:
                list_nav_prev(3);
                break;
            case 5:
                list_nav_prev(4);
                break;
            case 6:
            case 7:
                list_nav_prev(5);
                break;
            default:
                break;
        }
    }
}

static void handle_right(void) {
    if (layout_shown()) {
        layout_change(1);
        return;
    }

    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, +1, swap_axis);
        return;
    }

    if (menu_style_blocks(0)) return;
    set_nav_input_dir(nav_dir_right);
    if (spatial_move(1, 0)) return;
    if ((is_carousel_grid_mode() && theme.grid.row_count == 1) || menu_style_any_direction()) {
        list_nav_next(1);
        return;
    }

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_next(grid_row_right_steps(ui_count_dynamic));
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_next(ui_count_dynamic / 2);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (is_split_row_misc_nav()) {
        switch (current_item_index) {
            case 0:
                list_nav_next(3);
                break;
            case 1:
                list_nav_next(4);
                break;
            case 2:
                list_nav_next(5);
                break;
            default:
                break;
        }
    }
}

static void handle_left_hold(void) {
    if (layout_shown()) {
        layout_change(-1);
        return;
    }

    if (dialogue_active(&tour_dlg) || msgbox_active || dialogue_active(&confirm_dlg)) return;

    if (grid_row_hold_left_ok() || is_carousel_grid_mode() || menu_style_any_direction() || menu_style_spatial()) {
        handle_left();
    }
}

static void handle_right_hold(void) {
    if (layout_shown()) {
        layout_change(1);
        return;
    }

    if (dialogue_active(&tour_dlg) || msgbox_active || dialogue_active(&confirm_dlg)) return;

    if (grid_row_hold_right_ok() || is_carousel_grid_mode() || menu_style_any_direction() || menu_style_spatial()) {
        handle_right();
    }
}

static void layout_nav_visible(void) {
    const int swap = config.settings.remap.layout;
    const char *names[] = {"lr", swap ? "y" : "x", swap ? "x" : "y", swap ? "a" : "b", "start"};
    lv_obj_t *glyphs[] = {
        ui_lbl_nav_lr_glyph, ui_lbl_nav_x_glyph, ui_lbl_nav_y_glyph, ui_lbl_nav_b_glyph, ui_lbl_nav_start_glyph
    };
    lv_obj_t *labels[] = {ui_lbl_nav_lr, ui_lbl_nav_x, ui_lbl_nav_y, ui_lbl_nav_b, ui_lbl_nav_start};
    for (size_t i = 0; i < A_SIZE(glyphs); i++) {
        char path[MAX_BUFFER_SIZE];
        char embed[MAX_BUFFER_SIZE];
        if (!lv_img_get_src(glyphs[i])
            && generate_image_embed(mux_dim, "footer", names[i], path, sizeof(path), embed, sizeof(embed))) {
            set_footer_glyph_image(glyphs[i], embed);
        }
        lv_obj_set_style_img_opa(glyphs[i], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(labels[i], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_width(glyphs[i], LV_SIZE_CONTENT);
        lv_obj_set_width(labels[i], LV_SIZE_CONTENT);
    }
}

static void update_nav(void) {
    nav_hide_all();
    if (layout_edit) layout_nav_visible();

    if (layout_edit && !layout_panel) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}}
        );
        return;
    }

    if (layout_edit && !layout_shown()) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_y_glyph, "", 0},
                                      {ui_lbl_nav_y, lang.muxlaunch.layout.show, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.save, 0},
                                      {ui_lbl_nav_start_glyph, "", 0},
                                      {ui_lbl_nav_start, lang.muxlaunch.layout.discard, 0},
                                      {NULL, NULL, 0}});
        return;
    }

    if (layout_edit) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.muxlaunch.layout.change, 0},
                                      {ui_lbl_nav_x_glyph, "", 0},
                                      {ui_lbl_nav_x, lang.muxlaunch.layout.reset, 0},
                                      {ui_lbl_nav_y_glyph, "", 0},
                                      {ui_lbl_nav_y, lang.muxlaunch.layout.preview, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.save, 0},
                                      {ui_lbl_nav_start_glyph, "", 0},
                                      {ui_lbl_nav_start, lang.muxlaunch.layout.discard, 0},
                                      {NULL, NULL, 0}});
        return;
    }

    if (moving < 0) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0}, {ui_lbl_nav_a, lang.generic.select, 0}, {NULL, NULL, 0}}
        );
        return;
    }

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.muxlaunch.move_swap, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.cancel, 0},
                                  {NULL, NULL, 0}});
}

static void init_elements(void) {
    header_and_footer_setup();

    update_nav();

#define LAUNCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_launch, UDATA);
    LAUNCH_ELEMENTS
#undef LAUNCH

    overlay_display();
}

static mux_input_options launch_input_opts;

static void muxlaunch_setup(void) {
    moving = -1;
    page_dots[0] = page_dots[1] = NULL;
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlaunch.title);
    init_muxlaunch(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    init_navigation_group();
    if (layout_edit) {
        layout_open();
        update_nav();
    }

    if (file_exist(CONTENT_SWITCH_FAILED)) {
        remove(CONTENT_SWITCH_FAILED);
        toast_message(lang.muxmedia.playback_failed, tst_wait_m);
    }

    dialogue_init_confirm(
        &confirm_dlg, &theme, ui_screen, lang.muxlaunch.confirm_reboot, NULL, lang.generic.confirm, lang.generic.cancel,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_init_confirm(
        &tour_dlg, &theme, ui_screen, lang.generic.tour_offer, lang.generic.tour_offer_desc, lang.generic.yes,
        lang.generic.no, lang.generic.select, NULL
    );

    tour_dlg.safe_default = mux_confirm_yep;

    adjust_wallpaper_element(ui_group, 0, wall_general);

    init_timer(ui_gen_refresh_task, NULL);

    if (file_exist(MUOS_SYS_LOAD)) remove(MUOS_SYS_LOAD);

    launch_input_opts = (mux_input_options) {
        .swap_axis = (theme.grid.enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5)
                     || (!theme.grid.enabled && theme.misc.navigation_type >= 1 && theme.misc.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_x] = handle_x_press,
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
                [mux_input_start] = handle_start,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_x] = handle_x,
            [mux_input_y] = handle_y_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
        }
    };

    init_input(&launch_input_opts, 1);

    append_combo(
        &launch_input_opts, (mux_input_combo) {
                         .type_mask = BIT(mux_input_start) | BIT(mux_input_x),
                         .press_handler = handle_arc_press,
                         .hold_handler = handle_arc_hold,
                         .release_handler = handle_arc_release,
                     }
    );

    if (orientation_pending())
        show_tour_offer();
    else
        orientation_introduce(mux_module, lang.muxlaunch.title, lang.muxlaunch.overview);

}

int muxlaunch_main(void) {
    muxlaunch_setup();
    mux_input_task(&launch_input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}

int muxlaunch_host_start(void) {
    muxlaunch_setup();
    return mux_input_host_begin(&launch_input_opts);
}

void muxlaunch_host_end(void) {
    mux_input_host_end();
    if (item_count > 0) free_items(&items, &item_count);
}

int muxlaunch_layout_main(void) {
    layout_edit = 1;
    layout_panel = NULL;
    layout_page = 0;
    layout_timer = NULL;
    layout_pending = 0;
    const int result = muxlaunch_main();
    layout_edit = 0;
    layout_panel = NULL;
    menu_layout_edit(0);
    return result;
}
