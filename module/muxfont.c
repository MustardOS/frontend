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
static bool has_language_type;

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];

static void list_nav_move(int steps, int direction);

static bool detect_language_type(void) {
    char dir[MAX_BUFFER_SIZE];
    snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.SETTINGS.GENERAL.LANGUAGE);

    struct dirent **entries;
    int n = scandir(dir, &entries, NULL, alphasort);
    if (n < 0) return false;

    bool found = false;
    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;
        size_t len = strlen(name);
        if (!found && len > 4 && strcasecmp(name + len - 4, ".ttf") == 0) found = true;
        free(entries[i]);
    }
    free(entries);
    return found;
}

static int type_to_canonical(uint32_t dropdown_idx) {
    return has_language_type ? (int) dropdown_idx : (int) dropdown_idx + 1;
}

static uint32_t type_to_dropdown(int canonical) {
    if (!has_language_type) return (canonical > 0) ? (uint32_t) (canonical - 1) : 0;
    return (uint32_t) canonical;
}

static void refresh_navigation(void) {
    SHOW_OPTION_ITEM(font, Name);
    SHOW_OPTION_ITEM(font, ListSize);
    SHOW_OPTION_ITEM(font, HeaderSize);
    SHOW_OPTION_ITEM(font, FooterSize);
    SHOW_OPTION_ITEM(font, PanelSize);
    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
    if (type_to_canonical(lv_dropdown_get_selected(ui_droType_font)) == 1) {
        HIDE_OPTION_ITEM(font, Name);
        HIDE_OPTION_ITEM(font, ListSize);
        HIDE_OPTION_ITEM(font, HeaderSize);
        HIDE_OPTION_ITEM(font, FooterSize);
        HIDE_OPTION_ITEM(font, PanelSize);
    }
    list_nav_move(0, 1);
}

static void apply_current_font_settings(void) {
    config.SETTINGS.ADVANCED.FONT = (int16_t) type_to_canonical(lv_dropdown_get_selected(ui_droType_font));

    uint32_t idx;

    idx = lv_dropdown_get_selected(ui_droListSize_font);
    config.SETTINGS.FONT.LIST_SIZE = (idx < (uint32_t) font_size_count) ? (int16_t) font_size_values[idx] : 0;

    idx = lv_dropdown_get_selected(ui_droHeaderSize_font);
    config.SETTINGS.FONT.HEADER_SIZE = (idx < (uint32_t) font_size_count) ? (int16_t) font_size_values[idx] : 0;

    idx = lv_dropdown_get_selected(ui_droFooterSize_font);
    config.SETTINGS.FONT.FOOTER_SIZE = (idx < (uint32_t) font_size_count) ? (int16_t) font_size_values[idx] : 0;

    idx = lv_dropdown_get_selected(ui_droPanelSize_font);
    config.SETTINGS.FONT.PANEL_SIZE = (idx < (uint32_t) font_size_count) ? (int16_t) font_size_values[idx] : 0;

    lv_dropdown_get_selected_str(ui_droName_font, config.SETTINGS.FONT.NAME, sizeof(config.SETTINGS.FONT.NAME));

    lv_obj_remove_local_style_prop(ui_screen, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlContent, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlHeader, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);
    lv_obj_remove_local_style_prop(ui_pnlFooter, LV_STYLE_TEXT_FONT, MU_OBJ_MAIN_DEFAULT);

    init_fonts();

#define FONT(NAME, ENUM, UDATA) do { \
        const lv_font_t *_f = lv_obj_get_style_text_font(ui_dro##NAME##_font, LV_PART_MAIN); \
        lv_coord_t _h = lv_font_get_line_height(_f); \
        lv_obj_set_height(ui_dro##NAME##_font, _h); \
        if (theme.LIST_DEFAULT.LABEL_LONG_MODE != LV_LABEL_LONG_WRAP) { \
            lv_obj_set_height(ui_lbl##NAME##_font, _h); \
        } \
    } while (0);
    FONT_ELEMENTS
#undef FONT
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
    if (canonical_type == 2) {
        snprintf(dir, sizeof(dir), "%s", INTERNAL_FONTS);
    } else {
        snprintf(dir, sizeof(dir), INTERNAL_FONTS "/%s", config.SETTINGS.GENERAL.LANGUAGE);
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
    lv_dropdown_set_selected(ui_droType_font, type_to_dropdown(config.SETTINGS.ADVANCED.FONT));

    populate_font_names();

    int32_t name_idx = lv_dropdown_get_option_index(ui_droName_font, config.SETTINGS.FONT.NAME);
    if (name_idx >= 0) {
        lv_dropdown_set_selected(ui_droName_font, (uint32_t) name_idx);
    } else {
        lv_dropdown_set_selected(ui_droName_font, 0);
    }

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

    char name_current[MAX_BUFFER_SIZE];
    lv_dropdown_get_selected_str(ui_droName_font, name_current, sizeof(name_current));
    if (strcasecmp(name_current, name_original) != 0) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/font/name", "w", CHAR, name_current);
    }

    if (is_modified > 0) run_tweak_script(lang.GENERIC.SAVING);
}

static void init_navigation_group(void) {
    has_language_type = detect_language_type();

    char *all_type_options[] = {
            lang.MUXFONT.TYPE_OPTIONS.LANGUAGE,
            lang.MUXFONT.TYPE_OPTIONS.THEME,
            lang.MUXFONT.TYPE_OPTIONS.CUSTOM
    };
    char **type_options = has_language_type ? all_type_options : all_type_options + 1;
    int type_count = has_language_type ? 3 : 2;

    char *size_options = generate_number_string(6, 64, 2, lang.MUXFONT.SIZE_DEFAULT, NULL, NULL, 0);

    INIT_OPTION_ITEM(-1, font, Type, lang.MUXFONT.TYPE, "type", type_options, type_count);
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

    list_nav_move(direct_to_previous(ui_objects, ui_count, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, false, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
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
    if (msgbox_active) return;

    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    move_option(focused, +1);
    if (focused == ui_droType_font) {
        populate_font_names();
        refresh_navigation();
    }
    apply_current_font_settings();
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
}

static void handle_x(void) {
    if (msgbox_active) return;

    if (!hold_call) {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.HOLD_RESET, SHORT);
        return;
    }

    lv_dropdown_set_selected(ui_droType_font, type_to_dropdown(1));
    lv_dropdown_set_selected(ui_droListSize_font, 0);
    lv_dropdown_set_selected(ui_droHeaderSize_font, 0);
    lv_dropdown_set_selected(ui_droFooterSize_font, 0);
    lv_dropdown_set_selected(ui_droPanelSize_font, 0);

    int32_t noto_idx = lv_dropdown_get_option_index(ui_droName_font, "Noto Sans");
    lv_dropdown_set_selected(ui_droName_font, noto_idx >= 0 ? (uint32_t) noto_idx : 0);

    play_sound(SND_MUOS);
    apply_current_font_settings();
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    save_font_options();
    apply_current_font_settings();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "font");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

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

    init_fonts();
    init_elements();
    init_navigation_group();

    restore_font_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
