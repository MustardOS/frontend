#include "muxshare.h"
#include "ui/ui_muxtweakgen.h"

#define UI_COUNT 10

#define TWEAKGEN(NAME, UDATA) static int NAME##_original;
TWEAKGEN_ELEMENTS
#undef TWEAKGEN

static void list_nav_move(int steps, int direction);

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblRtc_tweakgen,        lang.MUXTWEAKGEN.HELP.DATETIME},
            {ui_lblHdmi_tweakgen,       lang.MUXTWEAKGEN.HELP.HDMI},
            {ui_lblAdvanced_tweakgen,   lang.MUXTWEAKGEN.HELP.ADVANCED},
            {ui_lblBrightness_tweakgen, lang.MUXTWEAKGEN.HELP.BRIGHT},
            {ui_lblVolume_tweakgen,     lang.MUXTWEAKGEN.HELP.VOLUME},
            {ui_lblColour_tweakgen,     lang.MUXTWEAKGEN.HELP.TEMP},
            {ui_lblRgb_tweakgen,        lang.MUXTWEAKGEN.HELP.RGB},
            {ui_lblHkDpad_tweakgen,     lang.MUXTWEAKGEN.HELP.HKDPAD},
            {ui_lblHkShot_tweakgen,     lang.MUXTWEAKGEN.HELP.HKSHOT},
            {ui_lblStartup_tweakgen,    lang.MUXTWEAKGEN.HELP.STARTUP},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define TWEAKGEN(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_tweakgen);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN
}

static void restore_tweak_options(void) {
    lv_dropdown_set_selected(ui_droBrightness_tweakgen, config.SETTINGS.GENERAL.BRIGHTNESS - 1);

    if (!config.SETTINGS.ADVANCED.OVERDRIVE) {
        lv_dropdown_set_selected(ui_droVolume_tweakgen, config.SETTINGS.GENERAL.VOLUME > 100
                                                        ? config.SETTINGS.GENERAL.VOLUME / 2
                                                        : config.SETTINGS.GENERAL.VOLUME);
    } else {
        lv_dropdown_set_selected(ui_droVolume_tweakgen, config.SETTINGS.GENERAL.VOLUME);
    }

    lv_dropdown_set_selected(ui_droColour_tweakgen, config.SETTINGS.GENERAL.COLOUR + 255);
    lv_dropdown_set_selected(ui_droRgb_tweakgen, config.SETTINGS.GENERAL.RGB);

    if (device.BOARD.STICK > 0) {
        lv_dropdown_set_selected(ui_droHkDpad_tweakgen, 0);
    } else {
        lv_dropdown_set_selected(ui_droHkDpad_tweakgen, config.SETTINGS.GENERAL.HKDPAD);
    }

    lv_dropdown_set_selected(ui_droHkShot_tweakgen, config.SETTINGS.GENERAL.HKSHOT);

    lv_dropdown_set_selected(ui_droStartup_tweakgen,
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "explore") == 0 ? 1 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "collection") == 0 ? 2 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "history") == 0 ? 3 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "last") == 0 ? 4 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "resume") == 0 ? 5 : 0);
}

static void set_setting_value(const char *script_name, int value, int offset) {
    char script_path[MAX_BUFFER_SIZE];
    snprintf(script_path, sizeof(script_path), OPT_PATH "script/device/%s.sh", script_name);

    char value_str[8];
    snprintf(value_str, sizeof(value_str), "%d", value + offset);

    if (!block_input) {
        block_input = 1;

        const char *args[] = {script_path, value_str, NULL};
        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

        block_input = 0;
    }
}

static void save_tweak_options(void) {
    int is_modified = 0;

    const char *startup_options[] = {
            "launcher",
            "explore",
            "collection",
            "history",
            "last",
            "resume"
    };

    CHECK_AND_SAVE_VAL(tweakgen, Startup, "settings/general/startup", CHAR, startup_options);
    CHECK_AND_SAVE_STD(tweakgen, Rgb, "settings/general/rgb", INT, 0);

    if (device.BOARD.STICK > 0) {
        HkDpad_original = -1;
        CHECK_AND_SAVE_STD(tweakgen, HkDpad, "settings/hotkey/dpad_toggle", INT, -1);
    } else {
        CHECK_AND_SAVE_STD(tweakgen, HkDpad, "settings/hotkey/dpad_toggle", INT, 0);
    }

    CHECK_AND_SAVE_STD(tweakgen, HkShot, "settings/hotkey/screenshot", INT, 0);

    if (lv_dropdown_get_selected(ui_droColour_tweakgen) != Colour_original) {
        is_modified++;
        write_text_to_file(CONF_CONFIG_PATH "settings/general/colour", "w", INT,
                           lv_dropdown_get_selected(ui_droColour_tweakgen) - 255);
    }

    if (lv_dropdown_get_selected(ui_droBrightness_tweakgen) != Brightness_original) {
        is_modified++;
        set_setting_value("bright.sh", lv_dropdown_get_selected(ui_droBrightness_tweakgen), 1);
    }

    if (lv_dropdown_get_selected(ui_droVolume_tweakgen) != Volume_original) {
        is_modified++;
        set_setting_value("audio.sh", lv_dropdown_get_selected(ui_droVolume_tweakgen), 0);
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_screen(ui_screen);

        const char *args[] = {(OPT_PATH "script/mux/tweak.sh"), NULL};
        run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

        refresh_config = 1;
    }
}

static char **load_combos(const char *filename, int *count) {
    int line_count = 0;
    char **combos = str_parse_file(filename, &line_count, LINES);
    if (!combos || line_count == 0) return NULL;

    char **hk_lines = calloc(line_count, sizeof(char *));
    if (!hk_lines) {
        for (int i = 0; i < line_count; i++) free(combos[i]);
        free(combos);
        return NULL;
    }

    for (int i = 0; i < line_count; i++) {
        char *combo = combos[i];

        char *open_bracket = strchr(combo, '[');
        char *close_bracket = strrchr(combo, ']');

        if (!open_bracket || !close_bracket || open_bracket >= close_bracket) continue;

        *close_bracket = '\0';
        char *scan_pos = open_bracket + 1;

        char buffer[128] = {0};
        char *write_pos = buffer;
        int is_first = 1;

        while (*scan_pos) {
            if (*scan_pos == '"') {
                char *entry_start = ++scan_pos;
                while (*scan_pos && *scan_pos != '"') scan_pos++;

                size_t entry_len = scan_pos - entry_start;
                if (entry_len > 0) {
                    if (!is_first) *write_pos++ = '+';

                    for (size_t j = 0; j < entry_len; j++) {
                        char c = entry_start[j];
                        *write_pos++ = (c == '_') ? ' ' : c;
                    }

                    is_first = 0;
                }
            }

            if (*scan_pos) scan_pos++;
        }

        *write_pos = '\0';
        hk_lines[i] = strdup(buffer);
    }

    for (int i = 0; i < line_count; i++) free(combos[i]);
    free(combos);

    *count = line_count;
    return hk_lines;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *startup_options[] = {
            lang.MUXTWEAKGEN.STARTUP.MENU,
            lang.MUXTWEAKGEN.STARTUP.EXPLORE,
            lang.MUXTWEAKGEN.STARTUP.COLLECTION,
            lang.MUXTWEAKGEN.STARTUP.HISTORY,
            lang.MUXTWEAKGEN.STARTUP.LAST,
            lang.MUXTWEAKGEN.STARTUP.RESUME
    };

    int hk_combo_count = 0;
    char **hk_combos = NULL;

    const char *combo_path = NULL;
    if (str_startswith(device.BOARD.NAME, "rg")) {
        combo_path = OPT_PATH "share/hotkey/rg.ini";
    } else if (str_startswith(device.BOARD.NAME, "tui")) {
        combo_path = OPT_PATH "share/hotkey/tui.ini";
    }

    if (combo_path) hk_combos = load_combos(combo_path, &hk_combo_count);

    INIT_OPTION_ITEM(-1, tweakgen, Rtc, lang.MUXTWEAKGEN.DATETIME, "clock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Hdmi, lang.MUXTWEAKGEN.HDMI, "hdmi", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Advanced, lang.MUXTWEAKGEN.ADVANCED, "advanced", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Brightness, lang.MUXTWEAKGEN.BRIGHT, "brightness", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Volume, lang.MUXTWEAKGEN.VOLUME, "volume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Colour, lang.MUXTWEAKGEN.TEMP, "colour", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Rgb, lang.MUXTWEAKGEN.RGB, "rgb", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakgen, HkDpad, lang.MUXTWEAKGEN.HKDPAD, "hkdpad", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, HkShot, lang.MUXTWEAKGEN.HKSHOT, "hkshot", hk_combos, hk_combo_count);

    INIT_OPTION_ITEM(-1, tweakgen, Startup, lang.MUXTWEAKGEN.STARTUP.TITLE, "startup", startup_options, 6);

    char *brightness_values = generate_number_string(1, device.SCREEN.BRIGHT, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droBrightness_tweakgen, brightness_values);
    free(brightness_values);

    char *volume_values = generate_number_string(device.AUDIO.MIN, device.AUDIO.MAX, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droVolume_tweakgen, volume_values);
    free(volume_values);

    char *colour_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droColour_tweakgen, colour_values);
    free(colour_values);

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

    if (!device.BOARD.HAS_HDMI) HIDE_OPTION_ITEM(tweakgen, Hdmi);
    if (!device.BOARD.RGB) HIDE_OPTION_ITEM(tweakgen, Rgb);
    if (device.BOARD.STICK > 0) HIDE_OPTION_ITEM(tweakgen, HkDpad);

    if (!hk_combo_count) {
        HIDE_OPTION_ITEM(tweakgen, HkDpad);
        HIDE_OPTION_ITEM(tweakgen, HkShot);
    }

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void nav_show_a(int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lblNavA, text);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void nav_show_lr(int show) {
    if (show) {
        lv_obj_clear_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lblNavLR, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavLRGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void check_focus(void) {
    struct _lv_obj_t *f = lv_group_get_focused(ui_group);

    int is_module = (f == ui_lblHdmi_tweakgen || f == ui_lblRtc_tweakgen || f == ui_lblAdvanced_tweakgen);
    int is_set_opt = (f == ui_lblBrightness_tweakgen || f == ui_lblVolume_tweakgen || f == ui_lblColour_tweakgen);

    if (is_module) {
        nav_show_a(1, lang.GENERIC.SELECT);
        nav_show_lr(0);
    } else if (is_set_opt) {
        nav_show_a(1, lang.GENERIC.SET);
        nav_show_lr(1);
    } else {
        nav_show_a(0, lang.GENERIC.SELECT);
        nav_show_lr(1);
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

static void update_option_values(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblBrightness_tweakgen) {
        toast_message(lang.MUXTWEAKGEN.BRIGHT_SET, SHORT);
        set_setting_value("bright", lv_dropdown_get_selected(ui_droBrightness_tweakgen), 1);
    } else if (element_focused == ui_lblVolume_tweakgen) {
        toast_message(lang.MUXTWEAKGEN.VOLUME_SET, SHORT);
        set_setting_value("audio", lv_dropdown_get_selected(ui_droVolume_tweakgen), 0);
    } else if (element_focused == ui_lblColour_tweakgen) {
        toast_message(lang.MUXTWEAKGEN.TEMP_SET, SHORT);
        set_setting_value("temp", lv_dropdown_get_selected(ui_droColour_tweakgen), -255);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_a(void) {
    if (msgbox_active || block_input || hold_call) return;

    struct {
        const char *glyph_name;
        const char *mux_name;
        int16_t *kiosk_flag;
    } elements[] = {
            {"clock",    "rtc",      &kiosk.DATETIME.CLOCK},
            {"hdmi",     "hdmi",     &kiosk.SETTING.HDMI},
            {"advanced", "tweakadv", &kiosk.SETTING.ADVANCED}
    };

    struct _lv_obj_t *ef = lv_group_get_focused(ui_group);
    const char *u_data = lv_obj_get_user_data(ef);

    for (size_t i = 0; i < A_SIZE(elements); i++) {
        if (strcasecmp(u_data, elements[i].glyph_name) == 0) {
            if (is_ksk(*elements[i].kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);

            save_tweak_options();
            load_mux(elements[i].mux_name);

            close_input();
            mux_input_stop();

            break;
        }
    }

    if (ef == ui_lblBrightness_tweakgen || ef == ui_lblVolume_tweakgen || ef == ui_lblColour_tweakgen) {
        update_option_values();
    } else {
        handle_option_next();
    }
}

static void handle_b(void) {
    if (block_input || hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "general");
    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || block_input || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void launch_danger(void) {
    if (msgbox_active || hold_call) return;

    if (lv_group_get_focused(ui_group) == ui_lblAdvanced_tweakgen) {
        load_mux("danger");

        close_input();
        mux_input_stop();
    }
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
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL, NULL,                            0}
    });

    check_focus();

#define TWEAKGEN(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_tweakgen, UDATA);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN

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

int muxtweakgen_main(void) {
    init_module("muxtweakgen");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKGEN.TITLE);
    init_muxtweakgen(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_L2) | BIT(MUX_INPUT_X),
                            .press_handler = launch_danger,
                            .hold_handler = launch_danger,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright_up,
                            .hold_handler = ui_common_handle_bright_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_SWITCH) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright_down,
                            .hold_handler = ui_common_handle_bright_down,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_volume_up,
                            .hold_handler = ui_common_handle_volume_up,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_volume_down,
                            .hold_handler = ui_common_handle_volume_down,
                    },
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
