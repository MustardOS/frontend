#include "muxshare.h"
#include "ui/ui_muxthemeopt.h"

#define THEMEOPT(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(THEMEOPT_ELEMENTS) };
#undef THEMEOPT

#define THEMEOPT(NAME, UDATA) static int NAME##_original;
THEMEOPT_ELEMENTS
#undef THEMEOPT

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

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

static int any_themeopt_modified(void) {
#define THEMEOPT(NAME, UDATA)                                                                                          \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_themeopt) != NAME##_original) return 1;
    THEMEOPT_ELEMENTS
#undef THEMEOPT
    return 0;
}

static int reset_mode = 0;
static mux_dialogue reset_dlg;

static void show_reset_dialog(void) {
    reset_mode = 1;
    reset_dlg.selected = 0;
    dialogue_show(&reset_dlg);
    dialogue_refresh(&reset_dlg, &theme);
}

static void hide_reset_dialog(void) {
    reset_mode = 0;
    dialogue_hide(&reset_dlg);
}

static const int16_t glyph_size_values[] = {-2, 0, -1, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128};

static void restore_glyph_dropdown(lv_obj_t *dropdown, const int16_t stored) {
    uint32_t idx = 0;

    for (size_t i = 0; i < A_SIZE(glyph_size_values); i++) {
        if (glyph_size_values[i] == stored) {
            idx = (uint32_t) i;
            break;
        }
    }

    lv_dropdown_set_selected(dropdown, idx);
}

static void
save_glyph_dropdown(const lv_obj_t *dropdown, const int original, const char *key, int16_t *cfg, int *is_modified) {
    const uint32_t sel = lv_dropdown_get_selected(dropdown);
    if ((int) sel == original) return;

    int16_t val = 0;
    if (sel < A_SIZE(glyph_size_values)) val = glyph_size_values[sel];

    write_text_to_file(key, "w", INT, (int) val);

    *cfg = val;
    (*is_modified)++;
}

static void restore_width_dropdown(lv_obj_t *dropdown, const int16_t stored) {
    uint32_t idx = 0;
    if (stored >= 10 && stored <= 100) idx = (uint32_t) (stored - 9);
    lv_dropdown_set_selected(dropdown, idx);
}

static void
save_width_dropdown(const lv_obj_t *dropdown, const int original, const char *key, int16_t *cfg, int *is_modified) {
    const uint32_t sel = lv_dropdown_get_selected(dropdown);
    if ((int) sel == original) return;
    const int16_t val = (int16_t) (sel >= 1 ? (int) sel + 9 : 0);
    write_text_to_file(key, "w", INT, (int) val);
    *cfg = val;
    (*is_modified)++;
}

static void do_reset(void) {
    lv_dropdown_set_selected(ui_dro_header_height_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_footer_height_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_content_item_count_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_glyph_list_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_glyph_footer_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_glyph_header_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_glyph_grid_themeopt, 0);
    lv_dropdown_set_selected(ui_dro_label_width_themeopt, 0);
    play_sound(snd_muos);
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define THEMEOPT(NAME, UDATA) {UDATA, lang.muxthemeopt.help.NAME},
        THEMEOPT_ELEMENTS
#undef THEMEOPT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define THEMEOPT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_themeopt);
    THEMEOPT_ELEMENTS
#undef THEMEOPT
}

static void restore_themeopt_options(void) {
    lv_dropdown_set_selected(ui_dro_header_height_themeopt, (uint32_t) (config.settings.themeopt.header_height + 1));
    lv_dropdown_set_selected(ui_dro_footer_height_themeopt, (uint32_t) (config.settings.themeopt.footer_height + 1));
    lv_dropdown_set_selected(
        ui_dro_content_item_count_themeopt, (uint32_t) config.settings.themeopt.content_item_count
    );

    restore_glyph_dropdown(ui_dro_glyph_list_themeopt, config.settings.themeopt.glyph_size_list);
    restore_glyph_dropdown(ui_dro_glyph_footer_themeopt, config.settings.themeopt.glyph_size_footer);
    restore_glyph_dropdown(ui_dro_glyph_header_themeopt, config.settings.themeopt.glyph_size_header);
    restore_glyph_dropdown(ui_dro_glyph_grid_themeopt, config.settings.themeopt.glyph_size_grid);
    restore_width_dropdown(ui_dro_label_width_themeopt, config.settings.themeopt.label_width);
}

static void save_themeopt_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(themeopt, header_height, "settings/theme/header_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, footer_height, "settings/theme/footer_height", INT, -1);
    CHECK_AND_SAVE_STD(themeopt, content_item_count, "settings/theme/content_item_count", INT, 0);

    save_glyph_dropdown(
        ui_dro_glyph_list_themeopt, glyph_list_original, CONF_CONFIG_PATH "settings/theme/glyph_size_list",
        &config.settings.themeopt.glyph_size_list, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_footer_themeopt, glyph_footer_original, CONF_CONFIG_PATH "settings/theme/glyph_size_footer",
        &config.settings.themeopt.glyph_size_footer, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_header_themeopt, glyph_header_original, CONF_CONFIG_PATH "settings/theme/glyph_size_header",
        &config.settings.themeopt.glyph_size_header, &is_modified
    );
    save_glyph_dropdown(
        ui_dro_glyph_grid_themeopt, glyph_grid_original, CONF_CONFIG_PATH "settings/theme/glyph_size_grid",
        &config.settings.themeopt.glyph_size_grid, &is_modified
    );
    save_width_dropdown(
        ui_dro_label_width_themeopt, label_width_original, CONF_CONFIG_PATH "settings/theme/label_width",
        &config.settings.themeopt.label_width, &is_modified
    );

    if (is_modified > 0) run_tweak_script(lang.generic.saving);
}

static void init_navigation_group(void) {
    char *height_options = generate_number_string(0, 64, 1, lang.muxthemeopt.size_default, NULL, NULL, 0);
    char *count_options = generate_number_string(1, 64, 1, lang.muxthemeopt.size_default, NULL, NULL, 0);
    char *width_options = generate_number_string(10, 100, 1, lang.muxthemeopt.size_default, "%", NULL, 1);

    char glyph_options[256];
    snprintf(
        glyph_options, sizeof(glyph_options), "%s\n%s\n%s\n8\n12\n16\n20\n24\n28\n32\n36\n40\n48\n56\n64\n80\n96\n128",
        lang.muxthemeopt.size_default, lang.muxthemeopt.glyph_auto, lang.muxthemeopt.glyph_native
    );

    INIT_OPTION_ITEM(-1, themeopt, header_height, lang.muxthemeopt.header_height, "headerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, footer_height, lang.muxthemeopt.footer_height, "footerheight", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, content_item_count, lang.muxthemeopt.content_item_count, "count", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, glyph_list, lang.muxthemeopt.glyph_list, "glyphlist", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, glyph_footer, lang.muxthemeopt.glyph_footer, "glyphfooter", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, glyph_header, lang.muxthemeopt.glyph_header, "glyphheader", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, glyph_grid, lang.muxthemeopt.glyph_grid, "glyphgrid", NULL, 0);
    INIT_OPTION_ITEM(-1, themeopt, label_width, lang.muxthemeopt.label_width, "labelwidth", NULL, 0);

    apply_theme_list_drop_down(&theme, ui_lbl_header_height_themeopt, ui_dro_header_height_themeopt, height_options);
    apply_theme_list_drop_down(&theme, ui_lbl_footer_height_themeopt, ui_dro_footer_height_themeopt, height_options);
    apply_theme_list_drop_down(
        &theme, ui_lbl_content_item_count_themeopt, ui_dro_content_item_count_themeopt, count_options
    );
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_list_themeopt, ui_dro_glyph_list_themeopt, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_footer_themeopt, ui_dro_glyph_footer_themeopt, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_header_themeopt, ui_dro_glyph_header_themeopt, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_glyph_grid_themeopt, ui_dro_glyph_grid_themeopt, glyph_options);
    apply_theme_list_drop_down(&theme, ui_lbl_label_width_themeopt, ui_dro_label_width_themeopt, width_options);

    free(height_options);
    free(count_options);
    free(width_options);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1, 2, 0, 1);
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, +1);
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
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

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (save_mode || reset_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode || reset_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_themeopt_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

        mux_input_stop();
        return;
    }

    if (reset_mode) {
        const mux_confirm_opt opt = (mux_confirm_opt) reset_dlg.selected;
        hide_reset_dialog();
        if (opt == mux_confirm_yep) do_reset();
        return;
    }

    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || reset_mode) return;

    if (config.settings.advanced.trust_remove) {
        do_reset();
        return;
    }

    play_sound(snd_confirm);
    show_reset_dialog();
}

static void handle_b(void) {
    if (hold_call) return;

    if (save_mode) {
        hide_save_dialog();
        return;
    }

    if (reset_mode) {
        hide_reset_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (!config.settings.advanced.trust_modify && any_themeopt_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);

    save_themeopt_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "themeopt");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode || reset_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.reset, 0},
                                  {NULL, NULL, 0}});

#define THEMEOPT(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_themeopt, UDATA);
    THEMEOPT_ELEMENTS
#undef THEMEOPT

    overlay_display();
}

int muxthemeopt_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxthemeopt.title);
    init_muxthemeopt(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_elements();
    init_navigation_group();
    first_open = 1;

    restore_themeopt_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &reset_dlg, &theme, ui_screen, lang.generic.confirm, NULL, lang.generic.reset, lang.generic.cancel,
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
                [mux_input_x] = handle_x,
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

    return 0;
}
