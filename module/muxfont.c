#include "muxshare.h"
#include "ui/ui_muxfont.h"

#define FONT(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(FONT_ELEMENTS)
};
#undef FONT

#define FONT(NAME, ENUM, UDATA) static int NAME##_original;
FONT_ELEMENTS
#undef FONT

static char name_original[MAX_BUFFER_SIZE];
static int has_language_type;
static int has_theme_type;

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];

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
    if ((int) config.SETTINGS.ADVANCED.FONT != Type_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droListSize_font) != ListSize_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droHeaderSize_font) != HeaderSize_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droFooterSize_font) != FooterSize_original) return 1;
    if ((int) lv_dropdown_get_selected(ui_droPanelSize_font) != PanelSize_original) return 1;
    if (config.SETTINGS.ADVANCED.FONT != 1) {
        char name_current[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_droName_font, name_current, sizeof(name_current));
        if (strcasecmp(name_current, name_original) != 0) return 1;
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
    snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.SETTINGS.GENERAL.LANGUAGE);

    struct dirent **entries;
    int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return 0;

    int found = 0;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        size_t len = strlen(name);
        if (!found && len > 4 && strcasecmp(name + len - 4, ".ttf") == 0) found = 1;
        free(entries[i]);
    }

    free(entries);
    return found;
}

static int type_to_canonical(uint32_t dropdown_idx) {
    if (dropdown_idx < (uint32_t) num_type_options) return dropdown_to_canonical[dropdown_idx];

    return dropdown_to_canonical[num_type_options - 1];
}

static uint32_t type_to_dropdown(int canonical) {
    for (int i = 0; i < num_type_options; i++) {
        if (dropdown_to_canonical[i] == canonical) return (uint32_t) i;
    }

    return (uint32_t) (num_type_options - 1);
}

static void refresh_navigation(void) {
    SHOW_OPTION_ITEM(font, Type);
    SHOW_OPTION_ITEM(font, Name);
    SHOW_OPTION_ITEM(font, ListSize);
    SHOW_OPTION_ITEM(font, HeaderSize);
    SHOW_OPTION_ITEM(font, FooterSize);
    SHOW_OPTION_ITEM(font, PanelSize);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (num_type_options <= 1) HIDE_OPTION_ITEM(font, Type);

    if (type_to_canonical(lv_dropdown_get_selected(ui_droType_font)) == 1) {
        HIDE_OPTION_ITEM(font, Name);
        HIDE_OPTION_ITEM(font, ListSize);
        HIDE_OPTION_ITEM(font, HeaderSize);
        HIDE_OPTION_ITEM(font, FooterSize);
        HIDE_OPTION_ITEM(font, PanelSize);
    }

    gen_step_movement(0, 1, 0, 0, 0);
}

static void apply_current_font_settings(void) {
    config.SETTINGS.ADVANCED.FONT = (int16_t) type_to_canonical(lv_dropdown_get_selected(ui_droType_font));

    uint32_t idx;

    idx = lv_dropdown_get_selected(ui_droListSize_font);
    config.SETTINGS.FONT.LIST_SIZE = (int16_t) ((idx < (uint32_t) font_size_count) ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_droHeaderSize_font);
    config.SETTINGS.FONT.HEADER_SIZE = (int16_t) ((idx < (uint32_t) font_size_count) ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_droFooterSize_font);
    config.SETTINGS.FONT.FOOTER_SIZE = (int16_t) ((idx < (uint32_t) font_size_count) ? font_size_values[idx] : 0);

    idx = lv_dropdown_get_selected(ui_droPanelSize_font);
    config.SETTINGS.FONT.PANEL_SIZE = (int16_t) ((idx < (uint32_t) font_size_count) ? font_size_values[idx] : 0);

    lv_dropdown_get_selected_str(ui_droName_font, config.SETTINGS.FONT.NAME, sizeof(config.SETTINGS.FONT.NAME));

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlContent, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlHeader, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlFooter, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts();
    lv_obj_invalidate(ui_screen);

#define FONT(NAME, ENUM, UDATA) do {                                                         \
        const lv_font_t *_f = lv_obj_get_style_text_font(ui_dro##NAME##_font, LV_PART_MAIN); \
        lv_coord_t _h = lv_font_get_line_height(_f);                                         \
        lv_obj_set_height(ui_dro##NAME##_font, _h);                                          \
        if (theme.LIST_DEFAULT.LABEL_LONG_MODE != LV_LABEL_LONG_WRAP)                        \
            lv_obj_set_height(ui_lbl##NAME##_font, _h);                                      \
    } while (0);
    FONT_ELEMENTS
#undef FONT
}

static void revert_font_settings(void) {
    config.SETTINGS.ADVANCED.FONT = (int16_t) Type_original;
    config.SETTINGS.FONT.LIST_SIZE = (int16_t) (ListSize_original < font_size_count ? font_size_values[ListSize_original] : 0);
    config.SETTINGS.FONT.HEADER_SIZE = (int16_t) (HeaderSize_original < font_size_count ? font_size_values[HeaderSize_original] : 0);
    config.SETTINGS.FONT.FOOTER_SIZE = (int16_t) (FooterSize_original < font_size_count ? font_size_values[FooterSize_original] : 0);
    config.SETTINGS.FONT.PANEL_SIZE = (int16_t) (PanelSize_original < font_size_count ? font_size_values[PanelSize_original] : 0);
    snprintf(config.SETTINGS.FONT.NAME, sizeof(config.SETTINGS.FONT.NAME), "%s", name_original);

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlContent, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlHeader, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlFooter, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts();
    lv_obj_invalidate(ui_screen);
}

static void show_help(void) {
    struct help_msg help_messages[] = {
#define FONT(NAME, ENUM, UDATA) { UDATA, lang.MUXFONT.HELP.ENUM },
            FONT_ELEMENTS
#undef FONT
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define FONT(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_font);
    FONT_ELEMENTS
#undef FONT

    Type_original = config.SETTINGS.ADVANCED.FONT;
    snprintf(name_original, sizeof(name_original), "%s", config.SETTINGS.FONT.NAME);
}

static void populate_font_names(void) {
    lv_dropdown_clear_options(ui_droName_font);

    char dir[MAX_BUFFER_SIZE];
    int canonical_type = type_to_canonical(lv_dropdown_get_selected(ui_droType_font));
    if (canonical_type == 0) {
        snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.SETTINGS.GENERAL.LANGUAGE);
    } else {
        snprintf(dir, sizeof(dir), "%s", INTERNAL_FONTS);
    }

    struct dirent **entries;
    int n = scandir(dir, &entries, NULL, alphasort);

    if (n < 0) {
        lv_dropdown_add_option(ui_droName_font, lang.MUXFONT.NONE, LV_DROPDOWN_POS_LAST);
        return;
    }

    int added = 0;
    for (int i = 0; i < n; i++) {
        const char *font_name = entries[i]->d_name;
        size_t len = strlen(font_name);
        if (len > 4 && strcasecmp(font_name + len - 4, ".ttf") == 0) {
            char name_no_ext[MAX_BUFFER_SIZE];
            snprintf(name_no_ext, sizeof(name_no_ext), "%.*s", (int) (len - 4), font_name);
            lv_dropdown_add_option(ui_droName_font, name_no_ext, LV_DROPDOWN_POS_LAST);
            added++;
        }
        free(entries[i]);
    }
    free(entries);

    if (!added) lv_dropdown_add_option(ui_droName_font, lang.MUXFONT.NONE, LV_DROPDOWN_POS_LAST);
}

static void restore_font_options(void) {
    int canonical = config.SETTINGS.ADVANCED.FONT;
    if (!has_theme_type && canonical == 1) canonical = 2;
    if (!has_language_type && canonical == 0) canonical = has_theme_type ? 1 : 2;

    config.SETTINGS.ADVANCED.FONT = (int16_t) canonical;
    lv_dropdown_set_selected(ui_droType_font, type_to_dropdown(canonical));

    populate_font_names();

    int32_t name_idx = lv_dropdown_get_option_index(ui_droName_font, config.SETTINGS.FONT.NAME);
    if (name_idx < 0) name_idx = lv_dropdown_get_option_index(ui_droName_font, "Noto Sans");
    lv_dropdown_set_selected(ui_droName_font, name_idx >= 0 ? (uint32_t) name_idx : 0);

    map_drop_down_to_index(ui_droListSize_font, config.SETTINGS.FONT.LIST_SIZE, font_size_values, font_size_count, 0);
    map_drop_down_to_index(ui_droHeaderSize_font, config.SETTINGS.FONT.HEADER_SIZE, font_size_values, font_size_count, 0);
    map_drop_down_to_index(ui_droFooterSize_font, config.SETTINGS.FONT.FOOTER_SIZE, font_size_values, font_size_count, 0);
    map_drop_down_to_index(ui_droPanelSize_font, config.SETTINGS.FONT.PANEL_SIZE, font_size_values, font_size_count, 0);

    refresh_navigation();
}

static void save_font_options(void) {
    int is_modified = 0;

    if ((int) config.SETTINGS.ADVANCED.FONT != Type_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/advanced/font", "w", INT, config.SETTINGS.ADVANCED.FONT);
    }
    CHECK_AND_SAVE_VAL(font, ListSize, "settings/font/list_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, HeaderSize, "settings/font/header_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, FooterSize, "settings/font/footer_size", INT, font_size_values);
    CHECK_AND_SAVE_VAL(font, PanelSize, "settings/font/panel_size", INT, font_size_values);

    if (config.SETTINGS.ADVANCED.FONT != 1) {
        char name_current[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(ui_droName_font, name_current, sizeof(name_current));
        if (strcasecmp(name_current, name_original) != 0) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, name_current);
        }
    }

    if (is_modified > 0) run_tweak_script(lang.GENERIC.SAVING);
}

static void init_navigation_group(void) {
    has_language_type = detect_language_type();
    has_theme_type = theme_has_font();

    char *all_type_options[] = {
            lang.MUXFONT.TYPE_OPTIONS.LANGUAGE,
            lang.MUXFONT.TYPE_OPTIONS.THEME,
            lang.MUXFONT.TYPE_OPTIONS.INTERNAL
    };

    num_type_options = 0;
    if (has_language_type) dropdown_to_canonical[num_type_options++] = 0;
    if (has_theme_type) dropdown_to_canonical[num_type_options++] = 1;
    dropdown_to_canonical[num_type_options++] = 2;

    char *type_options[3];
    for (int i = 0; i < num_type_options; i++) {
        type_options[i] = all_type_options[dropdown_to_canonical[i]];
    }

    char *size_options = generate_number_string(6, 64, 2, lang.MUXFONT.SIZE_DEFAULT, NULL, NULL, 0);

    INIT_OPTION_ITEM(-1, font, Type, lang.MUXFONT.TYPE, "type", type_options, num_type_options);
    INIT_OPTION_ITEM(-1, font, Name, lang.MUXFONT.NAME, "name", NULL, 0);
    INIT_OPTION_ITEM(-1, font, ListSize, lang.MUXFONT.LIST_SIZE, "listsize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, HeaderSize, lang.MUXFONT.HEADER_SIZE, "headersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, FooterSize, lang.MUXFONT.FOOTER_SIZE, "footersize", NULL, 0);
    INIT_OPTION_ITEM(-1, font, PanelSize, lang.MUXFONT.PANEL_SIZE, "panelsize", NULL, 0);

    populate_font_names();

    apply_theme_list_drop_down(&theme, ui_droListSize_font, size_options);
    apply_theme_list_drop_down(&theme, ui_droHeaderSize_font, size_options);
    apply_theme_list_drop_down(&theme, ui_droFooterSize_font, size_options);
    apply_theme_list_drop_down(&theme, ui_droPanelSize_font, size_options);

    free(size_options);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    gen_step_movement(direct_to_previous(ui_objects, ui_count, &nav_moved), +1, 0, 0, 1);
}


static void do_reset(void) {
    lv_dropdown_set_selected(ui_droType_font, type_to_dropdown(has_theme_type ? 1 : 2));
    lv_dropdown_set_selected(ui_droListSize_font, 0);
    lv_dropdown_set_selected(ui_droHeaderSize_font, 0);
    lv_dropdown_set_selected(ui_droFooterSize_font, 0);
    lv_dropdown_set_selected(ui_droPanelSize_font, 0);

    int32_t noto_idx = lv_dropdown_get_option_index(ui_droName_font, "Noto Sans");
    lv_dropdown_set_selected(ui_droName_font, noto_idx >= 0 ? (uint32_t) noto_idx : 0);

    play_sound(SND_MUOS);
    apply_current_font_settings();
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, -1);

    if (focused == ui_droType_font) {
        populate_font_names();
        refresh_navigation();
    }

    apply_current_font_settings();
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, +1);

    if (focused == ui_droType_font) {
        populate_font_names();
        refresh_navigation();
    }

    apply_current_font_settings();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (reset_mode) {
        if (!swap_axis) {
            dialogue_navigate(&reset_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
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
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) {
            save_font_options();
            apply_current_font_settings();
        } else {
            revert_font_settings();
        }

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "font");

        mux_input_stop();
        return;
    }

    if (reset_mode) {
        mux_confirm_opt opt = (mux_confirm_opt) reset_dlg.selected;
        hide_reset_dialog();
        if (opt == MUX_CONFIRM_YEP) do_reset();
        return;
    }

    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_x(void) {
    if (msgbox_active || save_mode || reset_mode) return;

    if (config.SETTINGS.ADVANCED.TRUSTREMOVE) {
        do_reset();
        return;
    }

    play_sound(SND_CONFIRM);
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

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_font_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);

    save_font_options();
    apply_current_font_settings();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "font");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode || reset_mode) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.RESET,  0},
            {NULL, NULL,                            0}
    });

#define FONT(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_font, UDATA);
    FONT_ELEMENTS
#undef FONT

    overlay_display();
}

int muxfont_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXFONT.TITLE);
    init_muxfont(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    nav_silent = 1;

    init_fonts();
    init_elements();
    init_navigation_group();

    restore_font_options();
    init_dropdown_settings();

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    dialogue_init_confirm(&reset_dlg, &theme, ui_screen, lang.GENERIC.CONFIRM, NULL,
                          lang.GENERIC.RESET, lang.GENERIC.CANCEL, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0, 1);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    nav_silent = 0;

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
