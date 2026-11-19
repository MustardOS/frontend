#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/display/colour_scheme.h>

#define SAMPLE_COUNT     12
#define SAMPLE_MAX       64
#define SAMPLE_FOLDERS   3
#define SAMPLE_LONG      3
#define LAYOUT_COUNT_MIN 3
#define LAYOUT_COUNT_MAX 30
#define PANEL_VISIBLE    7
#define ROW_SOURCE       (-1)
#define ROW_COLOUR       (-2)
#define SCHEME_LIMIT     64

static const char *const screen_modules[content_layout_count] = {"muxplore", "muxcollect", "muxhistory"};
static const char *const screen_glyphs[content_layout_count] = {"rom", "collection", "history"};

static content_layout_config layout_saved;

static lv_obj_t *panel;
static lv_obj_t *panel_list;
static lv_obj_t *panel_heading;
static lv_obj_t *panel_rows[content_field_count + 2];
static lv_obj_t *panel_values[content_field_count + 2];
static int row_ids[content_field_count + 2];
static int row_total;
static int row_height;
static int option_selected;

static int folder_mode;
static int folder_source;

static char schemes[SCHEME_LIMIT][MAX_BUFFER_SIZE];
static int scheme_total;
static int scheme_index;

static char sample_names[SAMPLE_MAX][MAX_BUFFER_SIZE];
static char *sample_labels[SAMPLE_MAX];
static char *sample_glyphs[SAMPLE_MAX];
static int sample_count;

static lv_timer_t *rebuild_timer;
static int rebuild_pending;

static content_layout_config *current_layout(void) {
    if (adjust_screen < 0 || adjust_screen >= content_layout_count) adjust_screen = content_layout_explore;
    if (folder_mode && folder_source) return content_layout_folder_config();

    return &config.settings.layout[adjust_screen];
}

static void set_folder_source(const int source) {
    if (source && !folder_source) *content_layout_folder_config() = config.settings.layout[content_layout_explore];

    folder_source = source;
    content_layout_folder_use(source);
}

static const char *scheme_base(const char *scheme) {
    const char *slash = strrchr(scheme, '/');
    return slash ? slash + 1 : scheme;
}

static const char *scheme_label(const int index) {
    const char *base = scheme_base(schemes[index]);

    for (int i = 0; i < scheme_total; i++) {
        if (i != index && strcmp(scheme_base(schemes[i]), base) == 0) return schemes[index];
    }

    return base;
}

static void load_schemes(void) {
    scheme_total = colour_scheme_names(schemes, SCHEME_LIMIT);
    scheme_index = 0;

    const char *current = folder_custom_scheme();
    for (int i = 0; i < scheme_total; i++) {
        if (strcmp(schemes[i], current) == 0) scheme_index = i + 1;
    }
}

static void build_rows(void) {
    row_total = 0;
    if (folder_mode) {
        row_ids[row_total++] = ROW_COLOUR;
        row_ids[row_total++] = ROW_SOURCE;
    }

    for (int i = 0; i < content_field_count; i++) {
        const int strip = i == content_field_strip_columns || i == content_field_strip_size;
        if (strip && adjust_screen == content_layout_history) continue;
        row_ids[row_total++] = i;
    }
}

static const char *option_name(const int option) {
    const char *names[] = {lang.muxadjust.style,         lang.muxadjust.columns,    lang.muxadjust.rows,
                           lang.muxadjust.cell_size,     lang.muxadjust.names,      lang.muxadjust.strip_columns,
                           lang.muxadjust.strip_size,    lang.muxadjust.side,       lang.muxadjust.offset_x,
                           lang.muxadjust.offset_y,      lang.muxadjust.item_count, lang.muxadjust.item_spacing,
                           lang.muxadjust.counter_align, lang.muxadjust.counter_x,  lang.muxadjust.counter_y};
    if (option == ROW_COLOUR) return lang.muxadjust.colour;
    return option == ROW_SOURCE ? lang.muxadjust.source : names[option];
}

static const char *option_help(const int option) {
    const char *helps[] = {
        lang.muxadjust.help.style,         lang.muxadjust.help.columns,    lang.muxadjust.help.rows,
        lang.muxadjust.help.cell_size,     lang.muxadjust.help.names,      lang.muxadjust.help.strip_columns,
        lang.muxadjust.help.strip_size,    lang.muxadjust.help.side,       lang.muxadjust.help.offset_x,
        lang.muxadjust.help.offset_y,      lang.muxadjust.help.item_count, lang.muxadjust.help.item_spacing,
        lang.muxadjust.help.counter_align, lang.muxadjust.help.counter_x,  lang.muxadjust.help.counter_y
    };
    if (option == ROW_COLOUR) return lang.muxadjust.help.colour;
    return option == ROW_SOURCE ? lang.muxadjust.help.source : helps[option];
}

static void option_text(char *out, const size_t size, const int option) {
    if (option == ROW_COLOUR) {
        snprintf(out, size, "%s", scheme_index ? scheme_label(scheme_index - 1) : lang.muxadjust.colour_global);
        return;
    }

    if (option == ROW_SOURCE) {
        snprintf(out, size, "%s", folder_source ? lang.muxadjust.source_folder : lang.muxadjust.source_screen);
        return;
    }

    const int value = *content_layout_value(current_layout(), option);
    const char *styles[] = {
        lang.muxadjust.style_theme,
        lang.muxadjust.style_list,
        lang.muxadjust.style_grid,
        lang.muxadjust.style_carousel,
        lang.muxadjust.style_carousel_vertical,
        lang.muxadjust.style_grid_list,
        lang.muxadjust.style_carousel_list
    };
    const char *names[] = {lang.muxadjust.names_bar, lang.muxadjust.names_cell, lang.muxadjust.names_hidden};
    const char *sides[] = {lang.muxadjust.side_full, lang.muxadjust.side_left, lang.muxadjust.side_right};

    if (option == content_field_style) {
        snprintf(out, size, "%s", styles[LV_CLAMP(0, value, content_style_count - 1)]);
    } else if (option == content_field_names) {
        snprintf(out, size, "%s", names[LV_CLAMP(0, value, content_names_count - 1)]);
    } else if (option == content_field_side) {
        snprintf(out, size, "%s", sides[LV_CLAMP(0, value, content_side_count - 1)]);
    } else if (option == content_field_counter_align) {
        const char *aligns[] = {
            lang.muxthemeopt.size_default, lang.muxadjust.counter_left, lang.muxadjust.counter_centre,
            lang.muxadjust.counter_right
        };
        snprintf(out, size, "%s", aligns[LV_CLAMP(-1, value, 2) + 1]);
    } else if (option == content_field_size || option == content_field_strip_size) {
        snprintf(out, size, "%d%%", value);
    } else if ((option == content_field_count_items || option == content_field_spacing)
               && value == content_layout_fields[option].fallback) {
        snprintf(out, size, "%s", lang.muxthemeopt.size_default);
    } else {
        snprintf(out, size, "%d", value);
    }
}

static void sample_add(const char *name, const char *glyph) {
    if (sample_count >= SAMPLE_MAX) return;

    snprintf(sample_names[sample_count], sizeof(sample_names[sample_count]), "%s", name);
    sample_labels[sample_count] = sample_names[sample_count];
    sample_glyphs[sample_count] = (char *) glyph;
    sample_count++;
}

static void free_names(char **names, const int count) {
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

static void folder_samples(void) {
    char **dir_names = NULL;
    char **dir_paths = NULL;
    char **file_names = NULL;
    char **file_paths = NULL;
    int *dir_counts = NULL;
    int dirs = 0;
    int files = 0;

    union_collect(adjust_folder, &dir_names, &dir_paths, &dirs, &file_names, &file_paths, &files, &dir_counts);

    const char *folder_file = config.visual.friendly_folder ? resolve_info_path("name/folder.json") : NULL;
    char *folder_json = folder_file ? read_all_char_from(folder_file) : NULL;
    const int folder_valid = folder_json && json_valid(folder_json);
    const struct json folder_names = folder_valid ? json_parse(folder_json) : (struct json) {0};

    content_item *found = NULL;
    size_t found_count = 0;

    for (int i = 0; i < dirs; i++) {
        char *friendly = get_friendly_folder_name(dir_names[i], folder_valid, folder_names);
        add_item(&found, &found_count, dir_names[i], friendly, dir_paths[i], content_type_folder);
        free(friendly);
    }

    for (int i = 0; i < files; i++) {
        char display[MAX_BUFFER_SIZE];
        resolve_friendly_name(file_paths[i], display);
        add_item(&found, &found_count, file_names[i], display, file_paths[i], content_type_item);
    }

    order_load(adjust_folder);
    order_prepare(found, found_count, adjust_folder);
    order_apply(found, found_count);

    for (size_t i = 0; i < found_count; i++) {
        const int folder = found[i].content_type == content_type_folder;
        sample_add(found[i].display_name, folder ? "folder" : screen_glyphs[content_layout_explore]);
    }

    free_items(&found, &found_count);
    free(folder_json);
    free(dir_counts);
    free_names(dir_names, dirs);
    free_names(dir_paths, dirs);
    free_names(file_names, files);
    free_names(file_paths, files);
}

static void build_samples(void) {
    sample_count = 0;
    if (folder_mode) folder_samples();
    if (sample_count) return;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        const int folder = adjust_screen != content_layout_history && i < SAMPLE_FOLDERS;

        if (folder) {
            snprintf(sample_names[i], sizeof(sample_names[i]), lang.muxadjust.sample_folder, i + 1);
        } else if (i == SAMPLE_LONG) {
            snprintf(sample_names[i], sizeof(sample_names[i]), "%s", lang.muxadjust.sample_long);
        } else {
            snprintf(sample_names[i], sizeof(sample_names[i]), lang.muxadjust.sample_item, i + 1);
        }

        sample_labels[i] = sample_names[i];
        sample_glyphs[i] = (char *) (folder ? "folder" : screen_glyphs[adjust_screen]);
    }

    sample_count = SAMPLE_COUNT;
}

static void preview_swap_axis(void) {
    swap_axis = theme.misc.navigation_type == 1
                || (grid_mode_enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5);
}

static void sample_focus(void) {
    if (grid_mode_enabled) {
        lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].name);
        return;
    }

    update_scroll_position(
        theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
    );
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
}

static void sample_move(const int steps, const int direction) {
    for (int step = 0; step < steps; step++) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        current_item_index = (current_item_index + direction + ui_count_static) % ui_count_static;

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        if (grid_mode_enabled) update_grid(direction);
    }

    play_sound(snd_navigate);
    sample_focus();
}

static void sample_prev(const int steps) {
    sample_move(steps, -1);
}

static void sample_next(const int steps) {
    sample_move(steps, +1);
}

static void sample_build(void) {
    lv_obj_clean(ui_pnl_content);
    lv_obj_clean(ui_pnl_grid);
    free_items(&items, &item_count);
    reset_ui_groups();
    content_strip_end();

    load_theme(&theme, &config, &device);
    init_panel_style(&theme);
    init_item_style(&theme);
    init_glyph_style(&theme);
    if (folder_mode) apply_screen_colours();

    lv_obj_set_height(ui_pnl_content, theme.misc.content.height);
    lv_obj_set_y(ui_pnl_content, theme.header.height + 2 + theme.misc.content.padding_top);
    lv_obj_set_style_pad_row(ui_pnl_content, theme.mux.item.spacing, MU_OBJ_MAIN_DEFAULT);
    content_strip_begin();

    current_item_index = 0;
    grid_mode_enabled = 0;
    ui_count_static = sample_count;

    if (theme.grid.enabled && (config.visual.grid_mode_content || content_layout_grid_forced(&config))) {
        lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_grid, LV_OBJ_FLAG_HIDDEN);
        init_grid_static(sample_count, sample_labels, sample_labels, sample_glyphs);
    } else {
        lv_obj_add_flag(ui_pnl_grid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnl_grid_current_item, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);

        for (int i = 0; i < sample_count; i++) {
            const int folder = strcmp(sample_glyphs[i], "folder") == 0;
            content_item *item = add_item(
                &items, &item_count, sample_labels[i], sample_labels[i], "",
                folder ? content_type_folder : content_type_item
            );
            if (item) item->glyph_icon = strdup(sample_glyphs[i]);
        }

        content_strip_split(screen_modules[adjust_screen], NULL);
        ui_count_static = (int) item_count;

        for (size_t i = 0; i < item_count; i++)
            gen_label(screen_modules[adjust_screen], items[i].glyph_icon, items[i].display_name);
    }

    sample_focus();
    content_strip_sync();

    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, NULL});
    if (panel) lv_obj_move_foreground(panel);
}

static void rebuild_settle(lv_timer_t *timer);
static void panel_colours(void);
static void panel_paint(void);

static void rebuild_now(void) {
    rebuild_pending = 0;
    sample_build();

    if (folder_mode && panel) {
        panel_colours();
        panel_paint();
    }

    rebuild_timer = lv_timer_create(rebuild_settle, 120, NULL);
    lv_timer_set_repeat_count(rebuild_timer, 1);
}

static void rebuild_settle(lv_timer_t *timer) {
    LV_UNUSED(timer);
    rebuild_timer = NULL;
    if (rebuild_pending) rebuild_now();
}

static void rebuild_queue(void) {
    if (rebuild_timer) {
        rebuild_pending = 1;
        return;
    }

    rebuild_now();
}

static void rebuild_flush(void) {
    if (!rebuild_timer) return;

    lv_timer_del(rebuild_timer);
    rebuild_timer = NULL;
    if (rebuild_pending) {
        rebuild_pending = 0;
        sample_build();
    }
}

static int panel_shown(void) {
    return panel && !lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

static void panel_paint(void) {
    for (int i = 0; i < row_total; i++) {
        const int selected = i == option_selected;

        char value[MAX_BUFFER_SIZE];
        option_text(value, sizeof(value), row_ids[i]);
        lv_label_set_text(panel_values[i], value);

        lv_obj_set_style_bg_opa(
            panel_rows[i], selected ? (lv_opa_t) theme.dialogue.selection_alpha : LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT
        );

        const lv_color_t text = lv_color_hex(selected ? theme.dialogue.option : theme.dialogue.content);
        lv_obj_t *labels[] = {lv_obj_get_child(panel_rows[i], 0), panel_values[i]};
        for (size_t j = 0; j < A_SIZE(labels); j++) {
            lv_obj_set_style_text_color(labels[j], text, MU_OBJ_MAIN_DEFAULT);
            if (selected) {
                lv_obj_add_state(labels[j], LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(labels[j], LV_STATE_CHECKED);
            }
        }
    }

    const int top = LV_CLAMP(0, option_selected - PANEL_VISIBLE / 2, LV_MAX(0, row_total - PANEL_VISIBLE));
    lv_obj_scroll_to_y(panel_list, (lv_coord_t) (top * row_height), LV_ANIM_OFF);
}

static void panel_colours(void) {
    if (!panel) return;

    lv_obj_set_style_bg_color(panel, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(panel, (lv_opa_t) theme.dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(panel, (lv_opa_t) theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_style_text_color(panel_heading, lv_color_hex(theme.dialogue.title), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(panel_heading, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(panel_heading, (lv_opa_t) theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);

    for (int i = 0; i < row_total; i++)
        lv_obj_set_style_bg_color(panel_rows[i], lv_color_hex(theme.dialogue.selection), MU_OBJ_MAIN_DEFAULT);
}

static void panel_create(void) {
    const char *titles[content_layout_count] = {
        lang.muxadjust.explore_title, lang.muxadjust.collection_title, lang.muxadjust.history_title
    };

    panel = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, (lv_coord_t) (device.mux.width * 0.8), LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(panel, 16, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(panel, 12, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(panel, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(panel, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_shadow_zone_register(
        panel, lv_color_hex(theme.dialogue.shadow_colour), (lv_opa_t) theme.dialogue.shadow_alpha,
        (int8_t) theme.dialogue.shadow_x_offset, (int8_t) theme.dialogue.shadow_y_offset,
        lv_color_hex(theme.dialogue.shadow_colour_focus), (lv_opa_t) theme.dialogue.shadow_alpha_focus,
        (int8_t) theme.dialogue.shadow_x_offset_focus, (int8_t) theme.dialogue.shadow_y_offset_focus
    );

    panel_heading = lv_label_create(panel);
    lv_obj_set_width(panel_heading, LV_PCT(100));
    lv_obj_set_style_text_align(panel_heading, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(panel_heading, folder_mode ? lang.generic.customise : titles[adjust_screen]);
    lv_obj_set_style_pad_ver(panel_heading, 8, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_side(panel_heading, LV_BORDER_SIDE_BOTTOM, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel_heading, 1, MU_OBJ_MAIN_DEFAULT);

    lv_obj_t *gap = lv_obj_create(panel);
    lv_obj_remove_style_all(gap);
    lv_obj_set_size(gap, LV_PCT(100), 6);

    const lv_font_t *font = lv_obj_get_style_text_font(panel, LV_PART_MAIN);
    row_height = lv_font_get_line_height(font) + 10;

    panel_list = lv_obj_create(panel);
    lv_obj_remove_style_all(panel_list);
    lv_obj_set_width(panel_list, LV_PCT(100));
    lv_obj_set_height(panel_list, (lv_coord_t) (LV_MIN(row_total, PANEL_VISIBLE) * row_height));
    lv_obj_set_flex_flow(panel_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel_list, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_scrollbar_mode(panel_list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < row_total; i++) {
        lv_obj_t *row = lv_obj_create(panel_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), (lv_coord_t) (row_height - 2));
        lv_obj_set_style_pad_hor(row, 10, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_radius(row, theme.dialogue.radius.selected, MU_OBJ_MAIN_DEFAULT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, option_name(row_ids[i]));
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        panel_values[i] = lv_label_create(row);
        lv_obj_align(panel_values[i], LV_ALIGN_RIGHT_MID, 0, 0);
        panel_rows[i] = row;
    }

    panel_colours();
    panel_paint();
}

static void update_nav(void) {
    nav_hide_all();

    if (!panel_shown()) {
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_y_glyph, "", 0},
                                      {ui_lbl_nav_y, lang.muxlaunch.layout.show, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
        return;
    }

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
}

static void panel_toggle(void) {
    rebuild_flush();

    if (panel_shown()) {
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        preview_swap_axis();
    } else {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
        swap_axis = 0;
    }

    play_sound(snd_confirm);
    update_nav();
}

static void option_move(const int direction) {
    option_selected = (option_selected + direction + row_total) % row_total;
    play_sound(snd_navigate);
    panel_paint();
}

static int stepped_value(const int index, const int current, const int direction) {
    const content_layout_field *option = &content_layout_fields[index];

    if (index == content_field_count_items) {
        if (current == 0) return direction > 0 ? LAYOUT_COUNT_MIN : LAYOUT_COUNT_MAX;

        const int value = current + direction;
        return value < LAYOUT_COUNT_MIN || value > LAYOUT_COUNT_MAX ? 0 : value;
    }

    int value = current;
    do {
        value += direction * option->step;
        if (value > option->max) value = option->min;
        if (value < option->min) value = option->max;
    } while (index == content_field_style && adjust_screen == content_layout_history
             && (value == content_style_grid_list || value == content_style_carousel_list));

    return value;
}

static void choose_scheme(const int index) {
    scheme_index = index;
    folder_custom_set_scheme(index ? schemes[index - 1] : "");
}

static void option_change(const int direction) {
    const int option = row_ids[option_selected];

    if (option == ROW_COLOUR) {
        choose_scheme((scheme_index + direction + scheme_total + 1) % (scheme_total + 1));
    } else if (option == ROW_SOURCE) {
        set_folder_source(!folder_source);
    } else {
        if (folder_mode) set_folder_source(1);

        int16_t *field = content_layout_value(current_layout(), option);
        *field = (int16_t) stepped_value(option, *field, direction);
    }

    play_sound(snd_navigate);
    panel_paint();
    rebuild_queue();
}

static void option_reset(void) {
    const int option = row_ids[option_selected];

    if (option == ROW_COLOUR) {
        choose_scheme(0);
    } else if (option == ROW_SOURCE) {
        set_folder_source(0);
    } else {
        if (folder_mode) set_folder_source(1);
        *content_layout_value(current_layout(), option) = (int16_t) content_layout_fields[option].fallback;
    }

    play_sound(snd_back);
    panel_paint();
    rebuild_queue();
}

static void save_layout(void) {
    if (folder_mode) {
        if (!folder_custom_save(adjust_folder, folder_source)) toast_message(lang.generic.save_fail, tst_wait_m);
        return;
    }

    if (memcmp(current_layout(), &layout_saved, sizeof(layout_saved)) == 0) return;

    if (!content_layout_save(&config)) toast_message(lang.generic.save_fail, tst_wait_m);
    refresh_config = 1;
}

static void leave(const int keep) {
    rebuild_flush();

    if (keep) {
        save_layout();
    } else if (!folder_mode) {
        *current_layout() = layout_saved;
    }

    if (!folder_mode) write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, content_layout_screens[adjust_screen]);
    play_sound(snd_back);
    mux_input_stop();
}

static void handle_up(void) {
    if (msgbox_active) return;

    if (panel_shown()) {
        option_move(-1);
        return;
    }

    if (content_strip_nav(1, -1)) return;
    handle_list_nav_up();
}

static void handle_down(void) {
    if (msgbox_active) return;

    if (panel_shown()) {
        option_move(+1);
        return;
    }

    if (content_strip_nav(1, +1)) return;
    handle_list_nav_down();
}

static void handle_left(void) {
    if (msgbox_active) return;

    if (panel_shown()) {
        option_change(-1);
        return;
    }

    if (content_strip_nav(0, -1)) return;
    handle_list_nav_left();
}

static void handle_right(void) {
    if (msgbox_active) return;

    if (panel_shown()) {
        option_change(+1);
        return;
    }

    if (content_strip_nav(0, +1)) return;
    handle_list_nav_right();
}

static void handle_up_hold(void) {
    if (panel_shown()) {
        handle_up();
        return;
    }

    if (content_strip_active()) return;
    handle_list_nav_up_hold();
}

static void handle_down_hold(void) {
    if (panel_shown()) {
        handle_down();
        return;
    }

    if (content_strip_active()) return;
    handle_list_nav_down_hold();
}

static void handle_left_hold(void) {
    if (panel_shown()) {
        handle_left();
        return;
    }

    if (content_strip_nav(0, -1)) return;
    handle_list_nav_left_hold();
}

static void handle_right_hold(void) {
    if (panel_shown()) {
        handle_right();
        return;
    }

    if (content_strip_nav(0, +1)) return;
    handle_list_nav_right_hold();
}

static void handle_x(void) {
    if (msgbox_active || !panel_shown()) return;

    option_reset();
}

static void handle_y(void) {
    if (msgbox_active) return;

    panel_toggle();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!panel_shown()) {
        panel_toggle();
        return;
    }

    leave(1);
}

static void handle_start(void) {
    if (msgbox_active || hold_call) return;

    leave(0);
}

static void handle_help(void) {
    if (msgbox_active || !panel_shown() || hold_call) return;

    play_sound(snd_info_open);
    show_info_box(option_name(row_ids[option_selected]), option_help(row_ids[option_selected]), 0);
}

int muxadjustscreen_main(void) {
    init_module(__func__);

    folder_mode = adjust_folder[0] != '\0';
    folder_source = 0;
    if (folder_mode) adjust_screen = content_layout_explore;

    current_layout();
    snprintf(mux_module, sizeof(mux_module), "%s", screen_modules[adjust_screen]);

    if (folder_mode) {
        folder_custom_load(adjust_folder);
        folder_source = content_layout_current(&config) == content_layout_folder_config();
        set_folder_source(1);
        load_schemes();
    } else {
        folder_custom_load(NULL);
    }

    build_rows();
    layout_saved = *current_layout();
    panel = NULL;
    option_selected = 0;
    rebuild_timer = NULL;
    rebuild_pending = 0;

    init_theme(1, 1);

    const char *screens[content_layout_count] = {
        lang.muxadjust.explore, lang.muxadjust.collection, lang.muxadjust.history
    };

    const char *folder_name = strrchr(adjust_folder, '/');
    init_ui_common_screen(
        &theme, &device, &lang, folder_mode ? (folder_name ? folder_name + 1 : adjust_folder) : screens[adjust_screen]
    );
    header_and_footer_setup();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    build_samples();
    sample_build();
    panel_create();
    update_nav();
    overlay_display();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = 0,
        .press_handler =
            {
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_start] = handle_start,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
        }
    };

    list_nav_set_callbacks(sample_prev, sample_next);
    init_input(&input_opts, 1);
    orientation_introduce("muxadjustscreen", screens[adjust_screen], lang.muxadjust.overview);

    mux_input_task(&input_opts);

    if (rebuild_timer) lv_timer_del(rebuild_timer);
    rebuild_timer = NULL;
    content_strip_end();
    content_layout_folder_use(0);
    free_items(&items, &item_count);

    return 0;
}
