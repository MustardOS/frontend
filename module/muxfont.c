#include "muxshare.h"
#include "ui/ui_muxfont.h"

#define FONT(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(FONT_ELEMENTS) };
#undef FONT

#define FONT(NAME, UDATA) static int NAME##_original;
FONT_ELEMENTS
#undef FONT

static char font_name_saved[MAX_BUFFER_SIZE];
static int has_language_type;
static int has_theme_type;

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

static int dropdown_to_canonical[3];
static int num_type_options;

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

static int any_font_modified(void) {
    if ((int) config.settings.advanced.font != type_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_list_size_font) != list_size_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_header_size_font) != header_size_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_footer_size_font) != footer_size_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_dro_panel_size_font) != panel_size_original) return 1;
    if (config.settings.advanced.font != 1) {
        char name_current[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_dro_name_font, name_current, sizeof(name_current));
        if (strcasecmp(name_current, font_name_saved) != 0) return 1;
    }
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

static int detect_language_type(void) {
    char dir[MAX_BUFFER_SIZE];
    snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.settings.general.language);

    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return 0;

    int found = 0;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        const size_t len = strlen(name);
        if (!found && len > 4 && strcasecmp(name + len - 4, ".ttf") == 0) found = 1;
        free(entries[i]);
    }

    free(entries);
    return found;
}

static int type_to_canonical(const uint32_t dropdown_idx) {
    if (dropdown_idx < (uint32_t) num_type_options) return dropdown_to_canonical[dropdown_idx];

    return dropdown_to_canonical[num_type_options - 1];
}

static uint32_t type_to_dropdown(const int canonical) {
    for (int i = 0; i < num_type_options; i++) {
        if (dropdown_to_canonical[i] == canonical) return (uint32_t) i;
    }

    return (uint32_t) (num_type_options - 1);
}

static void refresh_navigation(void) {
    SHOW_OPTION_ITEM(font, type);
    SHOW_OPTION_ITEM(font, name);
    SHOW_OPTION_ITEM(font, list_size);
    SHOW_OPTION_ITEM(font, header_size);
    SHOW_OPTION_ITEM(font, footer_size);
    SHOW_OPTION_ITEM(font, panel_size);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (num_type_options <= 1) HIDE_OPTION_ITEM(font, type);

    if (type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font)) == 1) {
        HIDE_OPTION_ITEM(font, name);
        HIDE_OPTION_ITEM(font, list_size);
        HIDE_OPTION_ITEM(font, header_size);
        HIDE_OPTION_ITEM(font, footer_size);
        HIDE_OPTION_ITEM(font, panel_size);
    }

    gen_step_movement(0, 1, 0, 0, 0);
}

static void apply_current_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));

    uint32_t idx = lv_dropdown_get_selected(ui_dro_list_size_font);
    config.settings.font.list_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_header_size_font);
    config.settings.font.header_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_footer_size_font);
    config.settings.font.footer_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_dro_panel_size_font);
    config.settings.font.panel_size = (int16_t) (idx < (uint32_t) font_size_count ? font_size_values[idx] : 0);

    lv_dropdown_get_selected_str(ui_dro_name_font, config.settings.font.name, sizeof(config.settings.font.name));

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts();
    lv_obj_invalidate(ui_screen);

#define FONT(NAME, UDATA)                                                                                              \
    do {                                                                                                               \
        const lv_font_t *_f = lv_obj_get_style_text_font(ui_dro_##NAME##_font, LV_PART_MAIN);                          \
        lv_coord_t _h = lv_font_get_line_height(_f);                                                                   \
        lv_obj_set_height(ui_dro_##NAME##_font, _h);                                                                   \
        if (theme.list_default.label_long_mode != LV_LABEL_LONG_WRAP) lv_obj_set_height(ui_lbl_##NAME##_font, _h);     \
    } while (0);
    FONT_ELEMENTS
#undef FONT
}

static void revert_font_settings(void) {
    config.settings.advanced.font = (int16_t) type_original;
    config.settings.font.list_size =
        (int16_t) (list_size_original < font_size_count ? font_size_values[list_size_original] : 0);
    config.settings.font.header_size =
        (int16_t) (header_size_original < font_size_count ? font_size_values[header_size_original] : 0);
    config.settings.font.footer_size =
        (int16_t) (footer_size_original < font_size_count ? font_size_values[footer_size_original] : 0);
    config.settings.font.panel_size =
        (int16_t) (panel_size_original < font_size_count ? font_size_values[panel_size_original] : 0);
    snprintf(config.settings.font.name, sizeof(config.settings.font.name), "%s", font_name_saved);

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_content, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_header, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnl_footer, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts();
    lv_obj_invalidate(ui_screen);
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define FONT(NAME, UDATA) {UDATA, lang.muxfont.help.NAME},
        FONT_ELEMENTS
#undef FONT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define FONT(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_font);
    FONT_ELEMENTS
#undef FONT

    type_original = config.settings.advanced.font;
    snprintf(font_name_saved, sizeof(font_name_saved), "%s", config.settings.font.name);
}

static void populate_font_names(void) {
    lv_dropdown_clear_options(ui_dro_name_font);

    char dir[MAX_BUFFER_SIZE];
    const int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_dro_type_font));
    if (canonical_type == 0) {
        snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.settings.general.language);
    } else {
        snprintf(dir, sizeof(dir), "%s", INTERNAL_FONTS);
    }

    struct dirent **entries;
    const int n = scandir(dir, &entries, NULL, alphasort);

    if (n < 0) {
        lv_dropdown_add_option(ui_dro_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);
        return;
    }

    int added = 0;
    for (int i = 0; i < n; i++) {
        const char *font_name = entries[i]->d_name;
        const size_t len = strlen(font_name);
        if (len > 4 && strcasecmp(font_name + len - 4, ".ttf") == 0) {
            char name_no_ext[MAX_BUFFER_SIZE];
            snprintf(name_no_ext, sizeof(name_no_ext), "%.*s", (int) (len - 4), font_name);
            lv_dropdown_add_option(ui_dro_name_font, name_no_ext, LV_DROPDOWN_POS_LAST);
            added++;
        }
        free(entries[i]);
    }
    free(entries);

    if (!added) lv_dropdown_add_option(ui_dro_name_font, lang.muxfont.none, LV_DROPDOWN_POS_LAST);
}

static void restore_font_options(void) {
    int canonical = config.settings.advanced.font;
    if (!has_theme_type && canonical == 1) canonical = 2;
    if (!has_language_type && canonical == 0) canonical = has_theme_type ? 1 : 2;

    config.settings.advanced.font = (int16_t) canonical;
    lv_dropdown_set_selected(ui_dro_type_font, type_to_dropdown(canonical));

    populate_font_names();

    int32_t name_idx = lv_dropdown_get_option_index(ui_dro_name_font, config.settings.font.name);
    if (name_idx < 0) name_idx = lv_dropdown_get_option_index(ui_dro_name_font, "Noto Sans");
    lv_dropdown_set_selected(ui_dro_name_font, name_idx >= 0 ? (uint32_t) name_idx : 0);

    map_drop_down_to_index(ui_dro_list_size_font, config.settings.font.list_size, font_size_values, font_size_count, 0);
    map_drop_down_to_index(
        ui_dro_header_size_font, config.settings.font.header_size, font_size_values, font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_footer_size_font, config.settings.font.footer_size, font_size_values, font_size_count, 0
    );
    map_drop_down_to_index(
        ui_dro_panel_size_font, config.settings.font.panel_size, font_size_values, font_size_count, 0
    );

    refresh_navigation();
}

static void save_font_options(void) {
    int is_modified = 0;

    if ((int) config.settings.advanced.font != type_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/advanced/font", "w", INT, config.settings.advanced.font);
    }
    CHECK_AND_SAVE_VAL(font, list_size, "settings/font/list_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, header_size, "settings/font/header_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, footer_size, "settings/font/footer_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, panel_size, "settings/font/panel_size", INT, font_size_values);

    if (config.settings.advanced.font != 1) {
        char name_current[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_dro_name_font, name_current, sizeof(name_current));
        if (strcasecmp(name_current, font_name_saved) != 0) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, name_current);
        }
    }

    if (is_modified > 0) run_tweak_script(lang.generic.saving);
}

static void init_navigation_group(void) {
    has_language_type = detect_language_type();
    has_theme_type = theme_has_font();

    char *all_type_options[] = {
        lang.muxfont.type_options.language, lang.muxfont.type_options.theme, lang.muxfont.type_options.internal
    };

    num_type_options = 0;
    if (has_language_type) dropdown_to_canonical[num_type_options++] = 0;
    if (has_theme_type) dropdown_to_canonical[num_type_options++] = 1;
    dropdown_to_canonical[num_type_options++] = 2;

    char *type_options[3];
    for (int i = 0; i < num_type_options; i++) {
        type_options[i] = all_type_options[dropdown_to_canonical[i]];
    }

    char *size_options = generate_number_string(6, 64, 2, lang.muxfont.size_default, NULL, NULL, 0);

    INIT_OPTION_ITEM(-1, font, type, lang.muxfont.type, "type", type_options, num_type_options);
    INIT_OPTION_ITEM(-1, font, name, lang.muxfont.name, "name", NULL, 0);
    INIT_OPTION_ITEM(-1, font, list_size, lang.muxfont.list_size, "listsize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, header_size, lang.muxfont.header_size, "headersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, footer_size, lang.muxfont.footer_size, "footersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, panel_size, lang.muxfont.panel_size, "panelsize", NULL, 0);

    populate_font_names();

    apply_theme_list_drop_down(&theme, ui_lbl_list_size_font, ui_dro_list_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_header_size_font, ui_dro_header_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_footer_size_font, ui_dro_footer_size_font, size_options);
    apply_theme_list_drop_down(&theme, ui_lbl_panel_size_font, ui_dro_panel_size_font, size_options);

    free(size_options);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    gen_step_movement(direct_to_previous(ui_objects, ui_count_static, &nav_moved), +1, 2, 0, 1);
}

static void do_reset(void) {
    lv_dropdown_set_selected(ui_dro_type_font, type_to_dropdown(has_theme_type ? 1 : 2));
    lv_dropdown_set_selected(ui_dro_list_size_font, 0);
    lv_dropdown_set_selected(ui_dro_header_size_font, 0);
    lv_dropdown_set_selected(ui_dro_footer_size_font, 0);
    lv_dropdown_set_selected(ui_dro_panel_size_font, 0);

    const int32_t noto_idx = lv_dropdown_get_option_index(ui_dro_name_font, "Noto Sans");
    lv_dropdown_set_selected(ui_dro_name_font, noto_idx >= 0 ? (uint32_t) noto_idx : 0);

    play_sound(snd_muos);
    apply_current_font_settings();
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

    if (focused == ui_dro_type_font) {
        populate_font_names();
        refresh_navigation();
    }

    apply_current_font_settings();
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

    if (focused == ui_dro_type_font) {
        populate_font_names();
        refresh_navigation();
    }

    apply_current_font_settings();
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

        if (opt == mux_unsaved_save) {
            save_font_options();
            apply_current_font_settings();
        } else {
            revert_font_settings();
        }

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "font");

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

    if (!config.settings.advanced.trust_modify && any_font_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);

    save_font_options();
    apply_current_font_settings();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "font");

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

#define FONT(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_font, UDATA);
    FONT_ELEMENTS
#undef FONT

    overlay_display();
}

int muxfont_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxfont.title);
    init_muxfont(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    nav_silent = 1;

    init_fonts();
    init_elements();
    init_navigation_group();

    restore_font_options();
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

    nav_silent = 0;

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
