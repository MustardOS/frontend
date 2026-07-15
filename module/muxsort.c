#include "muxshare.h"

static int default_original;
static int collection_original;
static int history_original;
static int *tag_originals = NULL;

static lv_obj_t **ui_objects_value = NULL;

static int save_mode = 0;
static mux_dialogue save_dlg;

static void show_save_dialog(void) {
    save_mode = 1;
    save_dlg.selected = 0;
    dialogue_show(&save_dlg);
    dialogue_refresh(&save_dlg, &theme);
}

static void hide_save_dialog(void) {
    save_mode = 0;
    dialogue_hide(&save_dlg);
}

static int any_sort_modified(void) {
    if (!ui_objects_value) return 0;
    if ((int) lv_dropdown_get_selected(ui_objects_value[0]) != default_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_objects_value[1]) != collection_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_objects_value[2]) != history_original) return 1;
    if (tag_originals) {
        for (size_t i = 0; i < tag_item_count; i++) {
            if ((int) lv_dropdown_get_selected(ui_objects_value[3 + i]) != tag_originals[i]) return 1;
        }
    }
    return 0;
}

static void show_help(void) {
    char *title = TRS(lv_label_get_text(lv_group_get_focused(ui_group)));

    if (current_item_index == 0) {
        show_info_box(title, lang.muxsort.help.dflt, 0);
    } else if (current_item_index == 1) {
        show_info_box(title, lang.muxsort.help.collection, 0);
    } else if (current_item_index == 2) {
        show_info_box(title, lang.muxsort.help.history, 0);
    } else {
        char help_message[MAX_BUFFER_SIZE];
        snprintf(help_message, sizeof(help_message), lang.muxsort.help.tag, title);
        show_info_box(title, help_message, 0);
    }
}

static void init_dropdown_settings(void) {
    default_original = lv_dropdown_get_selected(ui_objects_value[0]);
    collection_original = lv_dropdown_get_selected(ui_objects_value[1]);
    history_original = lv_dropdown_get_selected(ui_objects_value[2]);

    free(tag_originals);
    tag_originals = NULL;

    if (tag_item_count > 0) {
        tag_originals = mux_malloc(tag_item_count * sizeof(int));
        for (size_t i = 0; i < tag_item_count; i++) {
            tag_originals[i] = (int) lv_dropdown_get_selected(ui_objects_value[3 + i]);
        }
    }
}

static void restore_clamped_value(lv_obj_t *value, const int selected) {
    if (!value) return;

    int max = (int) lv_dropdown_get_option_cnt(value) - 1;
    if (max < 0) max = 0;

    lv_dropdown_set_selected(value, selected > max ? max : selected);
}

static void restore_sort_options(void) {
    restore_clamped_value(ui_objects_value[0], config.sort.dflt);
    restore_clamped_value(ui_objects_value[1], config.sort.collection);
    restore_clamped_value(ui_objects_value[2], config.sort.history);

    for (size_t i = 0; i < tag_item_count; i++) {
        restore_clamped_value(ui_objects_value[3 + i], tag_items[i].sort_bucket);
    }
}

static int save_sort_option(const lv_obj_t *ui_dro_item, const int original_value, char *file) {
    const int current = lv_dropdown_get_selected(ui_dro_item);
    if (current != original_value) {
        char config_path[MAX_BUFFER_SIZE];
        snprintf(config_path, sizeof(config_path), SORTING_CONFIG_PATH "%s", file);
        write_text_to_file(config_path, "w", INT, current);
        return 1;
    }
    return 0;
}

static void save_sort_options(void) {
    int is_modified = 0;

    is_modified += save_sort_option(ui_objects_value[0], default_original, "default");
    is_modified += save_sort_option(ui_objects_value[1], collection_original, "collection");
    is_modified += save_sort_option(ui_objects_value[2], history_original, "history");

    for (size_t i = 0; i < tag_item_count; i++) {
        is_modified += save_sort_option(
            ui_objects_value[3 + i], tag_originals ? tag_originals[i] : tag_items[i].sort_bucket, tag_items[i].glyph
        );
    }

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void add_tag_item(const int index, const char *label, const char *glyph, const char *options) {
    lv_obj_t *ui_pnl_item = lv_obj_create(ui_pnl_content);
    lv_obj_t *ui_lbl_item = lv_label_create(ui_pnl_item);
    lv_obj_t *ui_lbl_item_glyph = lv_img_create(ui_pnl_item);
    lv_obj_t *ui_dro_item = lv_dropdown_create(ui_pnl_item);

    apply_theme_list_panel(ui_pnl_item);
    apply_theme_list_item(&theme, ui_lbl_item, label);
    apply_theme_list_glyph(&theme, ui_lbl_item_glyph, mux_module, glyph);
    apply_theme_list_drop_down(&theme, ui_lbl_item, ui_dro_item, options);

    lv_group_add_obj(ui_group, ui_lbl_item);
    lv_group_add_obj(ui_group_value, ui_dro_item);
    lv_group_add_obj(ui_group_glyph, ui_lbl_item_glyph);
    lv_group_add_obj(ui_group_panel, ui_pnl_item);

    ui_objects_value[index] = ui_dro_item;
}

static void init_navigation_group(void) {
    reset_ui_groups();

    ui_count_static = (int) (3 + tag_item_count);
    ui_objects_value = malloc(ui_count_static * sizeof(lv_obj_t *));

    char *increment_values = generate_number_string(0, ui_count_static, 1, NULL, NULL, NULL, 0);

    add_tag_item(0, lang.muxsort.dflt, "default", increment_values);
    add_tag_item(1, lang.muxsort.collection, "collection", increment_values);
    add_tag_item(2, lang.muxsort.history, "history", increment_values);

    for (size_t i = 0; i < tag_item_count; i++) {
        add_tag_item((int) (i + 3), tag_items[i].name, tag_items[i].glyph, increment_values);
    }

    free(increment_values);
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_sort_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

        mux_input_stop();
        return;
    }

    if (msgbox_active) return;

    handle_option_next();
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_sort_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);
    save_sort_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxsort_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxsort.title);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    load_tag_items(&tag_items, &tag_item_count);
    sort_tag_items(tag_items, tag_item_count);
    init_navigation_group();

    restore_sort_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_left] = handle_option_prev,
                [mux_input_dpad_right] = handle_option_next,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_tag_items(&tag_items, &tag_item_count);

    free(tag_originals);
    tag_originals = NULL;

    free(ui_objects_value);
    ui_objects_value = NULL;

    return 0;
}
