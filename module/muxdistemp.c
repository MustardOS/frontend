#include "muxshare.h"
#include "ui/ui_muxdistemp.h"

#define DISTEMP(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(DISTEMP_ELEMENTS)
};
#undef DISTEMP

#define DISTEMP(NAME, ENUM, UDATA) static int NAME##_original;
DISTEMP_ELEMENTS
#undef DISTEMP

static int save_mode = 0;
static mux_dialogue save_dlg;

static lv_obj_t *ui_objects[UI_COUNT];
static lv_obj_t *ui_objects_value[UI_COUNT];
static lv_obj_t *ui_objects_glyph[UI_COUNT];
static lv_obj_t *ui_objects_panel[UI_COUNT];

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

static int any_distemp_modified(void) {
#define DISTEMP(NAME, ENUM, UDATA) if ((int) lv_dropdown_get_selected(ui_dro##NAME##_distemp) != NAME##_original) return 1;
    DISTEMP_ELEMENTS
#undef DISTEMP
    return 0;
}

static int schedule_enabled(void) {
    return (int) lv_dropdown_get_selected(ui_droSchedule_distemp) == 0;
}

static void show_help(void) {
    struct help_msg help_messages[UI_COUNT];
    int count = 0;

    help_messages[count++] = (struct help_msg) {"schedule", lang.MUXDISTEMP.HELP.SCHEDULE};

    if (schedule_enabled()) {
        help_messages[count++] = (struct help_msg) {"sunrisetemp", lang.MUXDISTEMP.HELP.SUNRISE_TEMP};
        help_messages[count++] = (struct help_msg) {"sunsettemp", lang.MUXDISTEMP.HELP.SUNSET_TEMP};
        help_messages[count++] = (struct help_msg) {"sunrisetime", lang.MUXDISTEMP.HELP.SUNRISE_TIME};
        help_messages[count++] = (struct help_msg) {"sunsettime", lang.MUXDISTEMP.HELP.SUNSET_TIME};
    } else {
        help_messages[count++] = (struct help_msg) {"temp", lang.MUXDISTEMP.HELP.TEMPERATURE};
    }

    gen_help(current_item_index, help_messages, (size_t) count, ui_group, items);
}

static void init_dropdown_settings(void) {
#define DISTEMP(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_distemp);
    DISTEMP_ELEMENTS
#undef DISTEMP
}

static void apply_colour_temp(int temp) {
    char *dev_path = read_all_char_from(CONF_DEVICE_PATH "screen/colour");
    if (!dev_path) return;

    char *end = dev_path + strlen(dev_path) - 1;
    while (end > dev_path && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';

    if (dev_path[0] && file_exist(dev_path)) {
        write_text_to_file(dev_path, "w", INT, temp);
    }

    free(dev_path);
}

static void restore_distemp_options(void) {
    lv_dropdown_set_selected(ui_droSchedule_distemp, config.SETTINGS.COLOUR.SCHEDULE_MODE);
    lv_dropdown_set_selected(ui_droSunriseTemp_distemp, config.SETTINGS.COLOUR.SUNRISE_TEMP + 255);
    lv_dropdown_set_selected(ui_droSunsetTemp_distemp, config.SETTINGS.COLOUR.SUNSET_TEMP + 255);
    lv_dropdown_set_selected(ui_droSunriseTime_distemp, config.SETTINGS.COLOUR.SUNRISE_TIME);
    lv_dropdown_set_selected(ui_droSunsetTime_distemp, config.SETTINGS.COLOUR.SUNSET_TIME);
    lv_dropdown_set_selected(ui_droTemp_distemp, config.SETTINGS.COLOUR.SUNRISE_TEMP + 255);
}


static void save_distemp_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(distemp, Schedule, "settings/colour/schedule_mode", INT, 0);

    if (schedule_enabled()) {
        CHECK_AND_SAVE_STD(distemp, SunriseTemp, "settings/colour/sunrise_temp", INT, -255);
        CHECK_AND_SAVE_STD(distemp, SunsetTemp, "settings/colour/sunset_temp", INT, -255);
        CHECK_AND_SAVE_STD(distemp, SunriseTime, "settings/colour/sunrise_time", INT, 0);
        CHECK_AND_SAVE_STD(distemp, SunsetTime, "settings/colour/sunset_time", INT, 0);
    } else {
        CHECK_AND_SAVE_STD(distemp, Temp, "settings/colour/sunrise_temp", INT, -255);
    }

    if (is_modified > 0) {
        refresh_config = 1;

        const char *sunrise_script[] = {OPT_PATH "script/init/async/S08sunrise.sh", NULL, NULL};

        if (schedule_enabled()) {
            sunrise_script[1] = "restart";
        } else {
            sunrise_script[1] = "stop";
        }

        if (file_exist(OPT_PATH "script/init/async/S08sunrise.sh")) {
            run_exec(sunrise_script, A_SIZE(sunrise_script), 1, 0, NULL, NULL);
        }

        if (!schedule_enabled()) {
            apply_colour_temp((int) lv_dropdown_get_selected(ui_droTemp_distemp) - 255);
        }
    }
}

static void refresh_navigation(void) {
    SHOW_OPTION_ITEM(distemp, SunriseTemp);
    SHOW_OPTION_ITEM(distemp, SunsetTemp);
    SHOW_OPTION_ITEM(distemp, SunriseTime);
    SHOW_OPTION_ITEM(distemp, SunsetTime);
    SHOW_OPTION_ITEM(distemp, Temp);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (schedule_enabled()) {
        HIDE_OPTION_ITEM(distemp, Temp);
    } else {
        HIDE_OPTION_ITEM(distemp, SunriseTemp);
        HIDE_OPTION_ITEM(distemp, SunsetTemp);
        HIDE_OPTION_ITEM(distemp, SunriseTime);
        HIDE_OPTION_ITEM(distemp, SunsetTime);
    }

    current_item_index = 0;
    gen_step_movement(0, +1, 0, 0, 0);
}

static void init_navigation_group(void) {
    char *schedule_options[] = {lang.GENERIC.ENABLED, lang.GENERIC.DISABLED};
    INIT_OPTION_ITEM(-1, distemp, Schedule, lang.MUXDISTEMP.SCHEDULE, "schedule", schedule_options, 2);

    INIT_OPTION_ITEM(-1, distemp, SunriseTemp, lang.MUXDISTEMP.SUNRISE_TEMP, "sunrisetemp", NULL, 0);
    char *temp_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSunriseTemp_distemp, temp_values);

    INIT_OPTION_ITEM(-1, distemp, SunsetTemp, lang.MUXDISTEMP.SUNSET_TEMP, "sunsettemp", NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSunsetTemp_distemp, temp_values);
    free(temp_values);

    INIT_OPTION_ITEM(-1, distemp, SunriseTime, lang.MUXDISTEMP.SUNRISE_TIME, "sunrisetime", NULL, 0);
    char *time_values = generate_time_string(15);
    apply_theme_list_drop_down(&theme, ui_droSunriseTime_distemp, time_values);

    INIT_OPTION_ITEM(-1, distemp, SunsetTime, lang.MUXDISTEMP.SUNSET_TIME, "sunsettime", NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSunsetTime_distemp, time_values);
    free(time_values);

    INIT_OPTION_ITEM(-1, distemp, Temp, lang.MUXDISTEMP.TEMPERATURE, "temp", NULL, 0);
    temp_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droTemp_distemp, temp_values);
    free(temp_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
}

static int is_temp_row(void) {
    lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    return focused == ui_droSunriseTemp_distemp || focused == ui_droSunsetTemp_distemp || focused == ui_droTemp_distemp;
}

static void refresh_x_nav(void) {
    int show = is_temp_row();
    struct nav_flag nav_e[] = {
            {ui_lblNavXGlyph, show},
            {ui_lblNavX,      show},
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));
    footer_nav_check_scroll();
}

static void handle_option_prev(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
    if (current_item_index == 0) refresh_navigation();
}

static void handle_option_next(void) {
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
    if (current_item_index == 0) refresh_navigation();
}

static void handle_option_prev_multi(void) {
    if (save_mode || msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), -25);
    if (current_item_index == 0) refresh_navigation();
}

static void handle_option_next_multi(void) {
    if (save_mode || msgbox_active || block_input) return;

    move_option(lv_group_get_focused(ui_group_value), +25);
    if (current_item_index == 0) refresh_navigation();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_up();
    refresh_x_nav();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        if (!swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(SND_NAVIGATE);
        }
        return;
    }

    handle_list_nav_down();
    refresh_x_nav();
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (save_mode) {
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) save_distemp_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "displaytemp");

        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

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

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_distemp_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);
    save_distemp_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "displaytemp");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void handle_x(void) {
    if (save_mode || msgbox_active || hold_call) return;
    if (!is_temp_row()) return;

    int temp = (int) lv_dropdown_get_selected(lv_group_get_focused(ui_group_value)) - 255;

    play_sound(SND_CONFIRM);
    apply_colour_temp(temp);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {ui_lblNavXGlyph,  "",                  0},
            {ui_lblNavX,       lang.GENERIC.SET,    0},
            {NULL, NULL,                            0}
    });

#define DISTEMP(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_distemp, UDATA);
    DISTEMP_ELEMENTS
#undef DISTEMP

    overlay_display();
}

int muxdistemp_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXDISTEMP.TITLE);
    init_muxdistemp(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_distemp_options();
    init_dropdown_settings();

    refresh_navigation();
    refresh_x_nav();

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0, 0);

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
                    [MUX_INPUT_L2] = handle_option_prev_multi,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
                    [MUX_INPUT_R2] = handle_option_next_multi,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
                    [MUX_INPUT_MENU] = handle_help,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_dpad_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_dpad_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
