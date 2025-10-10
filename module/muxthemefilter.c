#include "muxshare.h"
#include "ui/ui_muxthemefilter.h"

#define UI_COUNT 5

static char lookup_original_value[MAX_BUFFER_SIZE];

#define THEMEFILTER(NAME, UDATA) static int NAME##_original;
THEMEFILTER_ELEMENTS
#undef THEMEFILTER

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAllThemes_themefilter, lang.MUXTHEMEFILTER.HELP.COMPATIBILITY},
            {ui_lblGrid_themefilter,      lang.MUXTHEMEFILTER.HELP.GRID},
            {ui_lblHdmi_themefilter,      lang.MUXTHEMEFILTER.HELP.HDMI},
            {ui_lblLanguage_themefilter,  lang.MUXTHEMEFILTER.HELP.LANGUAGE},
            {ui_lblLookup_themefilter,    lang.MUXTHEMEFILTER.HELP.LOOKUP},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define THEMEFILTER(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_themefilter);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER
}

static void restore_theme_filter_options(void) {
    lv_dropdown_set_selected(ui_droAllThemes_themefilter, config.THEME.FILTER.ALL_THEMES);
    lv_dropdown_set_selected(ui_droGrid_themefilter, config.THEME.FILTER.GRID);
    lv_dropdown_set_selected(ui_droHdmi_themefilter, config.THEME.FILTER.HDMI);
    lv_dropdown_set_selected(ui_droLanguage_themefilter, config.THEME.FILTER.LANGUAGE);
}

static void save_theme_filter_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(themefilter, AllThemes, "theme/filter/allthemes", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, Grid, "theme/filter/grid", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, Hdmi, "theme/filter/hdmi", INT, 0);
    CHECK_AND_SAVE_STD(themefilter, Language, "theme/filter/language", INT, 0);

    if (strcmp(lookup_original_value, lv_label_get_text(ui_lblLookupValue_themefilter)) != 0) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "theme/filter/lookup"), "w", CHAR,
                           lv_label_get_text(ui_lblLookupValue_themefilter));
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_screen(ui_screen);
        refresh_config = 1;
        theme_down_index = 0;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *theme_compat_opt[] = {
            lang.MUXTHEMEFILTER.COMPAT.DEVICE,
            lang.MUXTHEMEFILTER.COMPAT.ALL,
    };

    INIT_OPTION_ITEM(-1, themefilter, AllThemes, lang.MUXTHEMEFILTER.COMPATIBILITY, "theme", theme_compat_opt, 2);
    INIT_OPTION_ITEM(-1, themefilter, Grid, lang.MUXTHEMEFILTER.GRID, "grid", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, themefilter, Hdmi, lang.MUXTHEMEFILTER.HDMI, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, themefilter, Language, lang.MUXTHEMEFILTER.LANGUAGE, "language", disabled_enabled, 2);

    INIT_VALUE_ITEM(-1, themefilter, Lookup, lang.MUXTHEMEFILTER.LOOKUP, "lookup", config.THEME.FILTER.LOOKUP);

    snprintf(lookup_original_value, sizeof(lookup_original_value), "%s", config.THEME.FILTER.LOOKUP);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void check_focus(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblLookup_themefilter) {
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;

    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) != ui_lblLookup_themefilter)
        decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_keyboard_OK_press(void) {
    key_show = 0;
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup_themefilter) {
        lv_label_set_text(ui_lblLookupValue_themefilter, lv_textarea_get_text(ui_txtEntry_themefilter));
    }

    reset_osk(key_entry);

    lv_textarea_set_text(ui_txtEntry_themefilter, "");
    lv_group_set_focus_cb(ui_group, NULL);
    lv_obj_add_flag(ui_pnlEntry_themefilter, LV_OBJ_FLAG_HIDDEN);
}

static void handle_keyboard_press(void) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_OK_press();
    } else if (strcmp(is_key, OSK_UPPER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_upper_map);
    } else if (strcmp(is_key, OSK_CHAR) == 0) {
        lv_btnmatrix_set_map(key_entry, key_special_map);
    } else if (strcmp(is_key, OSK_LOWER) == 0) {
        lv_btnmatrix_set_map(key_entry, key_lower_map);
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    if (lv_group_get_focused(ui_group) != ui_lblLookup_themefilter)
        increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    play_sound(SND_CONFIRM);

    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblLookup_themefilter) {
        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;

        lv_obj_clear_flag(ui_pnlEntry_themefilter, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_pnlEntry_themefilter);

        lv_textarea_set_text(ui_txtEntry_themefilter, lv_label_get_text(lv_group_get_focused(ui_group_value)));
    } else {
        handle_option_next();
    }
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    key_show ? handle_keyboard_press() : handle_confirm();
}

static void handle_x(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_backspace(ui_txtEntry_themefilter);
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_swap();
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

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txtEntry_themefilter, ui_pnlEntry_themefilter);
        return;
    }

    play_sound(SND_BACK);

    save_theme_filter_options();

    load_mux("themedwn");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || key_show || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void handle_up(void) {
    key_show ? key_up() : handle_list_nav_up();
}

static void handle_up_hold(void) {
    key_show ? key_up() : handle_list_nav_up_hold();
}

static void handle_down(void) {
    key_show ? key_down() : handle_list_nav_down();
}

static void handle_down_hold(void) {
    key_show ? key_down() : handle_list_nav_down_hold();
}

static void handle_left(void) {
    (key_show) ? key_left() : handle_option_prev();
}

static void handle_right(void) {
    (key_show) ? key_right() : handle_option_next();
}

static void handle_left_hold(void) {
    (key_show) ? key_left() : handle_option_prev();
}

static void handle_right_hold(void) {
    (key_show) ? key_right() : handle_option_next();
}

static void handle_l1(void) {
    if (!key_show) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (!key_show) handle_list_nav_page_down();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavAGlyph,  "",                  0},
            {ui_lblNavA,       lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.SAVE,   0},
            {NULL,             NULL,                0}
    });

    check_focus();

#define THEMEFILTER(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_themefilter, UDATA);
    THEMEFILTER_ELEMENTS
#undef THEMEFILTER

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxthemefilter_main(void) {
    init_module("muxthemefilter");

    init_theme(1, 0);
    init_ui_common_screen(&theme, &device, &lang, lang.MUXTHEMEFILTER.TITLE);
    init_muxthemefilter(ui_screen, ui_pnlContent, &theme);

    init_elements();
    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_theme_filter_options();
    init_dropdown_settings();

    init_osk(ui_pnlEntry_themefilter, ui_txtEntry_themefilter, false);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_down,
                    [MUX_INPUT_DPAD_LEFT] = handle_left,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_R1] = handle_r1,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_down_hold,
                    [MUX_INPUT_DPAD_LEFT] = handle_left_hold,
                    [MUX_INPUT_DPAD_RIGHT] = handle_right_hold,
                    [MUX_INPUT_L1] = handle_l1,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_r1,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
