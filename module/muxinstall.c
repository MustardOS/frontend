#include "muxshare.h"
#include "ui/ui_muxinstall.h"

#define INSTALL(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(INSTALL_ELEMENTS) };
#undef INSTALL

static void list_nav_move(int steps, int direction);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define INSTALL(NAME, UDATA) {UDATA, lang.muxinstall.help.NAME},
        INSTALL_ELEMENTS
#undef INSTALL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *item_labels[] = {
        lang.muxinstall.rtc, lang.muxinstall.language, lang.muxinstall.shutdown, lang.muxinstall.install
    };

    char *item_labels_short[] = {
        lang.muxinstall.abbr.rtc, lang.muxinstall.abbr.language, lang.muxinstall.abbr.shutdown,
        lang.muxinstall.abbr.install
    };

    char *glyph_names[] = {"clock", "language", "shutdown", "install"};

    reset_ui_groups();

    if (theme.grid.enabled) {
        list_nav_move(init_grid_static(ui_count_dynamic, item_labels, item_labels_short, glyph_names), +1);
    } else {
        INIT_STATIC_ITEM(-1, install, rtc, item_labels[0], glyph_names[0], 0);
        INIT_STATIC_ITEM(-1, install, language, item_labels[1], glyph_names[1], 0);
        INIT_STATIC_ITEM(-1, install, shutdown, item_labels[2], glyph_names[2], 0);
        INIT_STATIC_ITEM(-1, install, install, item_labels[3], glyph_names[3], 0);

        add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);
        list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
    }
}

static void list_nav_move(const int steps, const int direction) {
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

        if (grid_mode_enabled) update_grid(direction);
    }

    if (!grid_mode_enabled) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_dynamic, current_item_index, ui_pnl_content
        );
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    } else {
        lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].name);
    }

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    const struct {
        const char *glyph_name;
        const char *mux_name;
    } elements[] = {{"clock", "rtc"}, {"language", "language"}, {"shutdown", "shutdown"}, {"install", "install"}};

    struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(e_focused);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (strcmp(elements[i].mux_name, "fti-shutdown") != 0) {
                play_sound(snd_confirm);
            } else {
                toast_message(lang.generic.shutting_down, tst_wait_f);
            }

            load_mux(elements[i].mux_name);

            break;
        }
    }

    mux_input_stop();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
    }
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    if (msgbox_active) return;

    set_nav_input_dir(nav_dir_up);

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_up_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_up_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_next(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_next(misc_steps);
    } else {
        list_nav_prev(1);
    }
}

static void handle_down(void) {
    if (msgbox_active) return;

    set_nav_input_dir(nav_dir_down);

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_down_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_down_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_prev(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_prev(misc_steps);
    } else {
        list_nav_next(1);
    }
}

static void handle_up_hold(void) { // prev
    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_up_ok();
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_up_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index > 0;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok || is_carousel_grid_mode()) {
        handle_up();
    }
}

static void handle_down_hold(void) { // next
    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_down_ok(ui_count_dynamic);
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_down_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index < ui_count_dynamic - 1;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok || is_carousel_grid_mode()) {
        handle_down();
    }
}

static void handle_left(void) {
    if (msgbox_active) return;

    set_nav_input_dir(nav_dir_left);

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_prev(grid_row_left_steps());
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_prev(ui_count_dynamic / 2);
    }
}

static void handle_right(void) {
    if (msgbox_active) return;

    set_nav_input_dir(nav_dir_right);

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_next(grid_row_right_steps(ui_count_dynamic));
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_next(ui_count_dynamic / 2);
    }
}

static void handle_left_hold(void) {
    if (msgbox_active) return;

    if (grid_row_hold_left_ok()) handle_left();
}

static void handle_right_hold(void) {
    if (msgbox_active) return;

    if (grid_row_hold_right_ok()) handle_right();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav(
        (struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0}, {ui_lbl_nav_a, lang.generic.select, 0}, {NULL, NULL, 0}}
    );

#define INSTALL(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_install, UDATA);
    INSTALL_ELEMENTS
#undef INSTALL

    overlay_display();
}

int muxinstall_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxinstall.title);
    init_muxinstall(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    init_navigation_group();

    adjust_wallpaper_element(ui_group, 0, wall_general);

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = (theme.grid.enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5)
                     || (!theme.grid.enabled && theme.misc.navigation_type >= 1 && theme.misc.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
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

    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
