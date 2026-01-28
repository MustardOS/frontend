#include "muxshare.h"
#include "ui/ui_muxtweakgen.h"

#define UI_COUNT 9

#define TWEAKGEN(NAME, ENUM, UDATA) static int NAME##_original;
TWEAKGEN_ELEMENTS
#undef TWEAKGEN

static int audio_overdrive = 100;

static void list_nav_move(int steps, int direction);

static void show_help() {
    struct help_msg help_messages[] = {
#define TWEAKGEN(NAME, ENUM, UDATA) { ui_lbl##NAME##_tweakgen, lang.MUXTWEAKGEN.HELP.ENUM },
            TWEAKGEN_ELEMENTS
#undef TWEAKGEN
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static void init_audio_limits(void) {
    audio_overdrive = config.SETTINGS.ADVANCED.OVERDRIVE ? 200 : 100;
}

static int visible_hdmi(void) {
    return !lv_obj_has_flag(ui_pnlHdmi_tweakgen, LV_OBJ_FLAG_HIDDEN);
}

static void init_dropdown_settings(void) {
#define TWEAKGEN(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_tweakgen);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN

    Brightness_original = pct_to_int(lv_dropdown_get_selected(ui_droBrightness_tweakgen), 2, device.SCREEN.BRIGHT);
    Volume_original = pct_to_int(lv_dropdown_get_selected(ui_droVolume_tweakgen), 0, audio_overdrive);
}

static void restore_tweak_options(void) {
    lv_dropdown_set_selected(ui_droBrightness_tweakgen, int_to_pct(config.SETTINGS.GENERAL.BRIGHTNESS, 2, device.SCREEN.BRIGHT));
    lv_dropdown_set_selected(ui_droVolume_tweakgen, int_to_pct(config.SETTINGS.GENERAL.VOLUME, 0, audio_overdrive));
    lv_dropdown_set_selected(ui_droRgb_tweakgen, config.SETTINGS.GENERAL.RGB);
    lv_dropdown_set_selected(ui_droHkDpad_tweakgen, device.BOARD.HASSTICK > 0 ? 0 : config.SETTINGS.GENERAL.HKDPAD);
    lv_dropdown_set_selected(ui_droHkShot_tweakgen, config.SETTINGS.GENERAL.HKSHOT);

    lv_dropdown_set_selected(ui_droStartup_tweakgen,
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "explore") == 0 ? 1 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "collection") == 0 ? 2 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "history") == 0 ? 3 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "last") == 0 ? 4 :
                             strcasecmp(config.SETTINGS.GENERAL.STARTUP, "resume") == 0 ? 5 : 0);
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

    CHECK_AND_SAVE_STD(tweakgen, HkDpad, "settings/hotkey/dpad_toggle", INT, 0);
    CHECK_AND_SAVE_STD(tweakgen, HkShot, "settings/hotkey/screenshot", INT, 0);

    int bright_mod = pct_to_int(lv_dropdown_get_selected(ui_droBrightness_tweakgen), 2, device.SCREEN.BRIGHT);
    if (bright_mod != Brightness_original) set_setting_value("bright", bright_mod, 0);

    int volume_mod = pct_to_int(lv_dropdown_get_selected(ui_droVolume_tweakgen), 0, audio_overdrive);
    if (volume_mod != Volume_original) set_setting_value("audio", volume_mod, 0);

    if (is_modified > 0) run_tweak_script();
}

static char **load_combos(const char *filename, int *count) {
    int line_count = 0;
    char **combos = str_parse_file(filename, &line_count, PARSE_LINES);
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

        if (!open_bracket || !close_bracket || open_bracket >= close_bracket) {
            hk_lines[i] = NULL;
            continue;
        }

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

    INIT_OPTION_ITEM(-1, tweakgen, Rtc, lang.MUXTWEAKGEN.RTC, "clock", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Hdmi, lang.MUXTWEAKGEN.HDMI, "hdmi", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Advanced, lang.MUXTWEAKGEN.ADVANCED, "advanced", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Brightness, lang.MUXTWEAKGEN.BRIGHTNESS, "brightness", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Volume, lang.MUXTWEAKGEN.VOLUME, "volume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakgen, Rgb, lang.MUXTWEAKGEN.RGB, "rgb", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakgen, HkDpad, lang.MUXTWEAKGEN.HKDPAD, "hkdpad", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, HkShot, lang.MUXTWEAKGEN.HKSHOT, "hkshot", hk_combos, hk_combo_count);
    INIT_OPTION_ITEM(-1, tweakgen, Startup, lang.MUXTWEAKGEN.STARTUP.TITLE, "startup", startup_options, 6);

    char *bright_pct_values = generate_number_string(0, 100, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droBrightness_tweakgen, bright_pct_values);
    free(bright_pct_values);

    char *volume_pct_values = generate_number_string(0, audio_overdrive, 1, NULL, "%", NULL, 1);
    apply_theme_list_drop_down(&theme, ui_droVolume_tweakgen, volume_pct_values);
    free(volume_pct_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!device.BOARD.HASHDMI) HIDE_OPTION_ITEM(tweakgen, Hdmi);
    if (!device.BOARD.HASRGB) HIDE_OPTION_ITEM(tweakgen, Rgb);
    if (device.BOARD.HASSTICK > 0) HIDE_OPTION_ITEM(tweakgen, HkDpad);

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
    int is_set_opt = (f == ui_lblBrightness_tweakgen || f == ui_lblVolume_tweakgen);

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
    gen_step_movement(steps, direction, false, 0);
    check_focus();
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

#define HANDLE_TWEAK_OPT(TYPE, TOAST, VALUE, SCRIPT, OFFSET) \
    do {                                                     \
        if (element_focused == ui_lbl##TYPE##_tweakgen) {    \
            int v = (VALUE);                                 \
            if (v != TYPE##_original) {                      \
                toast_message(TOAST, SHORT);                 \
                set_setting_value(SCRIPT, v, (OFFSET));      \
            }                                                \
            return;                                          \
        }                                                    \
    } while (0)

static void update_option_values(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    HANDLE_TWEAK_OPT(Brightness, lang.MUXTWEAKGEN.BRIGHTNESS_SET,
                     pct_to_int(lv_dropdown_get_selected(ui_droBrightness_tweakgen), 2, device.SCREEN.BRIGHT), "bright", 0);
    HANDLE_TWEAK_OPT(Volume, lang.MUXTWEAKGEN.VOLUME_SET,
                     pct_to_int(lv_dropdown_get_selected(ui_droVolume_tweakgen), 0, audio_overdrive), "audio", 0);
}

static void handle_option_prev(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static int get_multi_count(void) {
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);

    if (element_focused == ui_lblBrightness_tweakgen) {
        return config.SETTINGS.ADVANCED.INCBRIGHT;
    } else if (element_focused == ui_lblVolume_tweakgen) {
        return config.SETTINGS.ADVANCED.INCVOLUME;
    }

    return 0;
}

static void handle_option_prev_multi(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -get_multi_count());
}

static void handle_option_next_multi(void) {
    if (msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +get_multi_count());
}

static void handle_a(void) {
    if (msgbox_active || block_input || hold_call) return;

    static int16_t KIOSK_PASS = 0;

    typedef enum {
        MENU_TOGGLE = 0,
        MENU_OPTION,
        MENU_CLOCK,
        MENU_HDMI,
        MENU_ADVANCED,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"rtc",      &kiosk.DATETIME.CLOCK,   MENU_CLOCK,    NULL},
            {"hdmi",     &kiosk.SETTING.HDMI,     MENU_HDMI, visible_hdmi},
            {"tweakadv", &kiosk.SETTING.ADVANCED, MENU_ADVANCED, NULL},
            {NULL,       &KIOSK_PASS,             MENU_OPTION,   NULL}, // Brightness
            {NULL,       &KIOSK_PASS,             MENU_OPTION,   NULL}, // Volume
            {NULL,       &KIOSK_PASS,             MENU_TOGGLE,   NULL}, // RGB Lights
            {NULL,       &KIOSK_PASS,             MENU_TOGGLE,   NULL}, // Hotkey DPAD
            {NULL,       &KIOSK_PASS,             MENU_TOGGLE,   NULL}, // Hotkey Screenshot
            {NULL,       &KIOSK_PASS,             MENU_TOGGLE,   NULL}, // Startup Mode
    };

    const menu_entry *visible_entries[UI_COUNT];
    size_t visible_count = 0;

    for (size_t i = 0; i < A_SIZE(entries); i++) {
        if (entries[i].visible && !entries[i].visible()) continue;
        visible_entries[visible_count++] = &entries[i];
    }

    if ((unsigned) current_item_index >= visible_count) return;
    const menu_entry *entry = visible_entries[current_item_index];

    switch (entry->action) {
        case MENU_CLOCK:
        case MENU_HDMI:
        case MENU_ADVANCED:
            if (is_ksk(*entry->kiosk_flag)) {
                kiosk_denied();
                return;
            }

            play_sound(SND_CONFIRM);
            save_tweak_options();
            load_mux(entry->mux_name);

            close_input();
            mux_input_stop();
            break;
        case MENU_OPTION:
            update_option_values();
            break;
        case MENU_TOGGLE:
            handle_option_next();
            break;
        default:
            return;
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
    show_help();
}

static void launch_danger(void) {
    if (msgbox_active || hold_call) return;

    if (lv_group_get_focused(ui_group) == ui_lblAdvanced_tweakgen) {
        load_mux("danger");

        close_input();
        mux_input_stop();
    }
}

static void init_elements(void) {
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

#define TWEAKGEN(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_tweakgen, UDATA);
    TWEAKGEN_ELEMENTS
#undef TWEAKGEN

    overlay_display();
}

int muxtweakgen_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKGEN.TITLE);
    init_muxtweakgen(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_tweak_options();
    init_dropdown_settings();
    init_audio_limits();

    init_timer(ui_gen_refresh_task, NULL);

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
                    [MUX_INPUT_L2] = handle_option_prev_multi,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_option_next_multi,
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
