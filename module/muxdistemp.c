#include "muxshare.h"
#include "ui/ui_muxdistemp.h"

#define DISTEMP(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(DISTEMP_ELEMENTS) };
#undef DISTEMP

#define DISTEMP(NAME, UDATA) static int NAME##_original;
DISTEMP_ELEMENTS
#undef DISTEMP

static int save_mode = 0;
static mux_dialogue save_dlg;

static lv_obj_t *ui_objects[ui_count_dynamic];
static lv_obj_t *ui_objects_value[ui_count_dynamic];
static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
static lv_obj_t *ui_objects_panel[ui_count_dynamic];

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
#define DISTEMP(NAME, UDATA)                                                                                           \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_distemp) != NAME##_original) return 1;
    DISTEMP_ELEMENTS
#undef DISTEMP
    return 0;
}

static int schedule_enabled(void) {
    return (int) lv_dropdown_get_selected(ui_dro_schedule_distemp) == 0;
}

static int schedule_times_valid(void) {
    if (!schedule_enabled()) return 1;
    const int sunrise = lv_dropdown_get_selected(ui_dro_sunrise_time_distemp);
    const int sunset = lv_dropdown_get_selected(ui_dro_sunset_time_distemp);
    if (sunrise >= sunset) {
        toast_message(lang.muxdistemp.invalid_time, tst_wait_m);
        return 0;
    }
    return 1;
}

static void show_help(void) {
    struct help_msg help_messages[ui_count_dynamic];
    int count = 0;

    help_messages[count++] = (struct help_msg) {"schedule", lang.muxdistemp.help.schedule};

    if (schedule_enabled()) {
        help_messages[count++] = (struct help_msg) {"sunrisetemp", lang.muxdistemp.help.sunrise_temp};
        help_messages[count++] = (struct help_msg) {"sunsettemp", lang.muxdistemp.help.sunset_temp};
        help_messages[count++] = (struct help_msg) {"sunrisetime", lang.muxdistemp.help.sunrise_time};
        help_messages[count++] = (struct help_msg) {"sunsettime", lang.muxdistemp.help.sunset_time};
    } else {
        help_messages[count++] = (struct help_msg) {"temp", lang.muxdistemp.help.temperature};
    }

    gen_help(current_item_index, help_messages, (size_t) count, ui_group, items);
}

static void init_dropdown_settings(void) {
#define DISTEMP(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_distemp);
    DISTEMP_ELEMENTS
#undef DISTEMP
}

static void apply_colour_temp(const int temp) {
    char *dev_path = read_all_char_from(CONF_DEVICE_PATH "screen/colour");
    if (!dev_path) return;

    char *end = dev_path + strlen(dev_path) - 1;
    while (end > dev_path && (*end == '\n' || *end == '\r' || *end == ' '))
        *end-- = '\0';

    if (dev_path[0] && file_exist(dev_path)) {
        write_text_to_file(dev_path, "w", INT, temp);
    }

    free(dev_path);
}

static void restore_distemp_options(void) {
    lv_dropdown_set_selected(ui_dro_schedule_distemp, config.settings.colour.schedule_mode);
    lv_dropdown_set_selected(ui_dro_sunrise_temp_distemp, config.settings.colour.sunrise_temp + 255);
    lv_dropdown_set_selected(ui_dro_sunset_temp_distemp, config.settings.colour.sunset_temp + 255);
    lv_dropdown_set_selected(ui_dro_sunrise_time_distemp, config.settings.colour.sunrise_time);
    lv_dropdown_set_selected(ui_dro_sunset_time_distemp, config.settings.colour.sunset_time);
    lv_dropdown_set_selected(ui_dro_temp_distemp, config.settings.colour.sunrise_temp + 255);
}

static void save_distemp_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(distemp, schedule, "settings/colour/schedule_mode", INT, 0);

    if (schedule_enabled()) {
        CHECK_AND_SAVE_STD(distemp, sunrise_temp, "settings/colour/sunrise_temp", INT, -255);
        CHECK_AND_SAVE_STD(distemp, sunset_temp, "settings/colour/sunset_temp", INT, -255);
        CHECK_AND_SAVE_STD(distemp, sunrise_time, "settings/colour/sunrise_time", INT, 0);
        CHECK_AND_SAVE_STD(distemp, sunset_time, "settings/colour/sunset_time", INT, 0);
    } else {
        CHECK_AND_SAVE_STD(distemp, temp, "settings/colour/sunrise_temp", INT, -255);
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
            apply_colour_temp((int) lv_dropdown_get_selected(ui_dro_temp_distemp) - 255);
        }
    }
}

static void refresh_navigation(void) {
    SHOW_OPTION_ITEM(distemp, sunrise_temp);
    SHOW_OPTION_ITEM(distemp, sunset_temp);
    SHOW_OPTION_ITEM(distemp, sunrise_time);
    SHOW_OPTION_ITEM(distemp, sunset_time);
    SHOW_OPTION_ITEM(distemp, temp);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (schedule_enabled()) {
        HIDE_OPTION_ITEM(distemp, temp);
    } else {
        HIDE_OPTION_ITEM(distemp, sunrise_temp);
        HIDE_OPTION_ITEM(distemp, sunset_temp);
        HIDE_OPTION_ITEM(distemp, sunrise_time);
        HIDE_OPTION_ITEM(distemp, sunset_time);
    }

    current_item_index = 0;
    gen_step_movement(0, +1, 0, 0, 0);
}

static void init_navigation_group(void) {
    char *schedule_options[] = {lang.generic.enabled, lang.generic.disabled};
    INIT_OPTION_ITEM(-1, distemp, schedule, lang.muxdistemp.schedule, "schedule", schedule_options, 2);

    INIT_OPTION_ITEM(-1, distemp, sunrise_temp, lang.muxdistemp.sunrise_temp, "sunrisetemp", NULL, 0);
    char *temp_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_sunrise_temp_distemp, temp_values);

    INIT_OPTION_ITEM(-1, distemp, sunset_temp, lang.muxdistemp.sunset_temp, "sunsettemp", NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_sunset_temp_distemp, temp_values);
    free(temp_values);

    INIT_OPTION_ITEM(-1, distemp, sunrise_time, lang.muxdistemp.sunrise_time, "sunrisetime", NULL, 0);
    char *time_values = generate_time_string(15);
    apply_theme_list_drop_down(&theme, ui_dro_sunrise_time_distemp, time_values);

    INIT_OPTION_ITEM(-1, distemp, sunset_time, lang.muxdistemp.sunset_time, "sunsettime", NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_sunset_time_distemp, time_values);
    free(time_values);

    INIT_OPTION_ITEM(-1, distemp, temp, lang.muxdistemp.temperature, "temp", NULL, 0);
    temp_values = generate_number_string(-255, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_temp_distemp, temp_values);
    free(temp_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static int is_temp_row(void) {
    const lv_obj_t *focused = lv_group_get_focused(ui_group_value);
    return focused == ui_dro_sunrise_temp_distemp || focused == ui_dro_sunset_temp_distemp
           || focused == ui_dro_temp_distemp;
}

static void refresh_x_nav(void) {
    const int show = is_temp_row();
    const struct nav_flag nav_e[] = {
        {ui_lbl_nav_x_glyph, show},
        {ui_lbl_nav_x, show},
    };
    set_nav_flags(nav_e, A_SIZE(nav_e));
    footer_nav_check_scroll();
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
    if (current_item_index == 0) refresh_navigation();
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
            play_sound(snd_navigate);
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
            play_sound(snd_navigate);
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
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) {
            if (!schedule_times_valid()) {
                play_sound(snd_error);
                return;
            }
            save_distemp_options();
        }

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
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

    if (!config.settings.advanced.trust_modify && any_distemp_modified()) {
        show_save_dialog();
        return;
    }

    if (any_distemp_modified() && !schedule_times_valid()) {
        play_sound(snd_error);
        return;
    }

    play_sound(snd_back);
    save_distemp_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "displaytemp");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || save_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_x(void) {
    if (save_mode || msgbox_active || hold_call) return;
    if (!is_temp_row()) return;

    const int temp = (int) lv_dropdown_get_selected(lv_group_get_focused(ui_group_value)) - 255;

    play_sound(snd_confirm);
    apply_colour_temp(temp);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.set, 0},
                                  {NULL, NULL, 0}});

#define DISTEMP(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_distemp, UDATA);
    DISTEMP_ELEMENTS
#undef DISTEMP

    overlay_display();
}

int muxdistemp_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxdistemp.title);
    init_muxdistemp(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_distemp_options();
    init_dropdown_settings();

    refresh_navigation();
    refresh_x_nav();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
    );

    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 0, 0, 0);

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
                [mux_input_l2] = handle_option_prev_multi,
                [mux_input_r1] = handle_list_nav_page_down,
                [mux_input_r2] = handle_option_next_multi,
            },
        .release_handler =
            {
                [mux_input_l2] = hold_call_release,
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_left] = handle_option_prev,
            [mux_input_dpad_right] = handle_option_next,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_l2] = hold_call_set,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
