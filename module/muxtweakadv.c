#include "muxshare.h"
#include "ui/ui_muxtweakadv.h"

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

#define TWEAKADV(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(TWEAKADV_ELEMENTS) };
#undef TWEAKADV

#define TWEAKADV(NAME, UDATA) static int NAME##_original;
TWEAKADV_ELEMENTS
#undef TWEAKADV

static int any_tweakadv_modified(void) {
#define TWEAKADV(NAME, UDATA)                                                                                          \
    if (lv_dropdown_get_selected(ui_dro_##NAME##_tweakadv) != NAME##_original) return 1;
    TWEAKADV_ELEMENTS
#undef TWEAKADV
    return 0;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define TWEAKADV(NAME, UDATA) {UDATA, lang.muxtweakadv.help.NAME},
        TWEAKADV_ELEMENTS
#undef TWEAKADV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define TWEAKADV(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_tweakadv);
    TWEAKADV_ELEMENTS
#undef TWEAKADV
}

static void restore_tweak_options(void) {
#define TWEAKADV(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_tweakadv, config.settings.advanced.NAME);
    TWEAKADV_ELEMENTS
#undef TWEAKADV

    lv_dropdown_set_selected(ui_dro_second_part_tweakadv, device.storage.sdcard.partition - 1);
    lv_dropdown_set_selected(ui_dro_usb_part_tweakadv, device.storage.usb.partition - 1);
    lv_dropdown_set_selected(ui_dro_inc_bright_tweakadv, config.settings.advanced.inc_bright - 1);
    lv_dropdown_set_selected(ui_dro_inc_volume_tweakadv, config.settings.advanced.inc_volume - 1);
    lv_dropdown_set_selected(ui_dro_bt_scan_timeout_tweakadv, config.settings.advanced.bt_scan_timeout / 5 - 1);
    lv_dropdown_set_selected(ui_dro_usb_function_tweakadv, config.settings.advanced.usb_function);

    map_drop_down_to_index(ui_dro_accelerate_tweakadv, config.settings.advanced.accelerate, accelerate_values, 17, 6);
    map_drop_down_to_index(
        ui_dro_repeat_delay_tweakadv, config.settings.advanced.repeat_delay, repeat_delay_values, 33, 13
    );
    map_drop_down_to_index(ui_dro_swapfile_tweakadv, config.settings.advanced.swapfile, swap_values, 11, 0);
    map_drop_down_to_index(ui_dro_zramfile_tweakadv, config.settings.advanced.zramfile, swap_values, 11, 0);
}

static void normalise_overdrive(const int overdrive_old, const int overdrive_new) {
    if (overdrive_old && !overdrive_new) {
        const int current = read_line_int_from(CONF_CONFIG_PATH "settings/general/volume", 1);

        if (current > 100) {
            const int new_volume = current * 100 / 200;

            write_text_to_file(CONF_CONFIG_PATH "settings/general/volume", "w", INT, new_volume);
            set_setting_value("audio", new_volume, 0);
        }
    }
}

static void save_tweak_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(tweakadv, stick_nav, "settings/advanced/sticknav", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, volume, "settings/advanced/volume", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, brightness, "settings/advanced/brightness", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, thermal, "settings/advanced/thermal", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, led, "settings/advanced/led", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, random_theme, "settings/advanced/random_theme", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, retro_wait, "settings/advanced/retrowait", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, retro_free, "settings/advanced/retrofree", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, retro_cache, "settings/advanced/retrocache", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, activity, "settings/advanced/activity", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, verbose, "settings/advanced/verbose", INT, 0);

    do {
        const int debuglog_current = lv_dropdown_get_selected(ui_dro_debug_log_tweakadv);
        if (debuglog_current != debug_log_original) {
            is_modified++;
            write_text_to_file(DEBUG_FILE, "w", INT, debuglog_current);
            nop_debug_mode();
        }
    } while (0);

    CHECK_AND_SAVE_STD(tweakadv, rumble, "settings/advanced/rumble", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, user_init, "settings/advanced/user_init", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, dpad_swap, "settings/advanced/dpad_swap", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, lid_switch, "settings/advanced/lidswitch", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, disp_suspend, "settings/advanced/disp_suspend", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, stage_overlay, "settings/advanced/stage_overlay", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, max_gpu, "settings/advanced/maxgpu", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, double_buffer, "settings/advanced/double_buffer", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, audio_ready, "settings/advanced/audio_ready", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, audio_swap, "settings/advanced/audio_swap", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, audio_suspend, "settings/advanced/audio_suspend", INT, 0);

    do {
        const int bt_scan_current = lv_dropdown_get_selected(ui_dro_bt_scan_timeout_tweakadv);
        if (bt_scan_current != bt_scan_timeout_original) {
            is_modified++;
            write_text_to_file(
                CONF_CONFIG_PATH "settings/advanced/bt_scan_timeout", "w", INT, (bt_scan_current + 1) * 5
            );
        }
    } while (0);

    CHECK_AND_SAVE_STD(tweakadv, trust_modify, "settings/advanced/trust_modify", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, trust_power, "settings/advanced/trust_power", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, trust_remove, "settings/advanced/trust_remove", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, usb_function, "settings/advanced/usb_function", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, box_art_pad_div, "settings/advanced/boxartpaddiv", INT, 0);

    do {
        const int sd2_current = lv_dropdown_get_selected(ui_dro_second_part_tweakadv);
        if (sd2_current != second_part_original) {
            is_modified++;
            write_text_to_file(CONF_DEVICE_PATH "storage/sdcard/num", "w", INT, sd2_current + 1);
        }
    } while (0);

    do {
        const int usb_current = lv_dropdown_get_selected(ui_dro_usb_part_tweakadv);
        if (usb_current != usb_part_original) {
            is_modified++;
            write_text_to_file(CONF_DEVICE_PATH "storage/usb/num", "w", INT, usb_current + 1);
        }
    } while (0);

    do {
        const int bright_current = lv_dropdown_get_selected(ui_dro_inc_bright_tweakadv);
        if (bright_current != inc_bright_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/incbright", "w", INT, bright_current + 1);
        }
    } while (0);

    do {
        const int volume_current = lv_dropdown_get_selected(ui_dro_inc_volume_tweakadv);
        if (volume_current != inc_volume_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/incvolume", "w", INT, volume_current + 1);
        }
    } while (0);

    do {
        const int overdrive_current = lv_dropdown_get_selected(ui_dro_overdrive_tweakadv);
        if (overdrive_current != overdrive_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/overdrive", "w", INT, overdrive_current);
            normalise_overdrive(overdrive_original, overdrive_current);
        }
    } while (0);

    CHECK_AND_SAVE_MAP(tweakadv, accelerate, "settings/advanced/accelerate", accelerate_values, 17, 6);
    CHECK_AND_SAVE_MAP(tweakadv, repeat_delay, "settings/advanced/repeat_delay", repeat_delay_values, 33, 13);
    CHECK_AND_SAVE_MAP(tweakadv, swapfile, "settings/advanced/swapfile", swap_values, 11, 0);
    CHECK_AND_SAVE_MAP(tweakadv, zramfile, "settings/advanced/zramfile", swap_values, 11, 0);

    if (is_modified > 0) run_tweak_script(lang.generic.saving);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *sticknav_options[] = {
        lang.muxtweakadv.sticknav.dpad,    lang.muxtweakadv.sticknav.ls,      lang.muxtweakadv.sticknav.rs,
        lang.muxtweakadv.sticknav.dpad_ls, lang.muxtweakadv.sticknav.dpad_rs, lang.muxtweakadv.sticknav.dpad_ls_rs,
        lang.muxtweakadv.sticknav.ls_rs,
    };

    char *volume_options[] = {
        lang.generic.previous, lang.muxtweakadv.volume.silent, lang.muxtweakadv.volume.soft,
        lang.muxtweakadv.volume.loud
    };

    char *brightness_options[] = {
        lang.generic.previous, lang.muxtweakadv.brightness.low, lang.muxtweakadv.brightness.medium,
        lang.muxtweakadv.brightness.high
    };

    char *rumble_options[] = {lang.generic.disabled,       lang.muxtweakadv.rumble.st,   lang.muxtweakadv.rumble.sh,
                              lang.muxtweakadv.rumble.sl,  lang.muxtweakadv.rumble.stsh, lang.muxtweakadv.rumble.stsl,
                              lang.muxtweakadv.rumble.shsl};

    char *usb_functions[] = {lang.generic.disabled, lang.muxtweakadv.adb, lang.muxtweakadv.mtp};

    INIT_OPTION_ITEM(-1, tweakadv, accelerate, lang.muxtweakadv.accelerate, "accelerate", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, repeat_delay, lang.muxtweakadv.repeatdelay, "repeat", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, stick_nav, lang.muxtweakadv.sticknav.title, "sticknav", sticknav_options, 7);
    INIT_OPTION_ITEM(-1, tweakadv, volume, lang.muxtweakadv.volume.title, "volume", volume_options, 4);
    INIT_OPTION_ITEM(-1, tweakadv, brightness, lang.muxtweakadv.brightness.title, "brightness", brightness_options, 4);
    INIT_OPTION_ITEM(-1, tweakadv, thermal, lang.muxtweakadv.thermal, "thermal", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, led, lang.muxtweakadv.led, "led", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, random_theme, lang.muxtweakadv.randomtheme, "randomtheme", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, retro_wait, lang.muxtweakadv.retrowait, "retrowait", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, retro_free, lang.muxtweakadv.retrofree, "retrofree", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, retro_cache, lang.muxtweakadv.retrocache, "retrocache", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, activity, lang.muxtweakadv.activity, "activity", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, verbose, lang.muxtweakadv.verbose, "verbose", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, debug_log, lang.muxtweakadv.debuglog, "debuglog", debug_log_mode, 3);
    INIT_OPTION_ITEM(-1, tweakadv, rumble, lang.muxtweakadv.rumble.title, "rumble", rumble_options, 7);
    INIT_OPTION_ITEM(-1, tweakadv, user_init, lang.muxtweakadv.userinit, "userinit", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, dpad_swap, lang.muxtweakadv.dpadswap, "dpadswap", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, overdrive, lang.muxtweakadv.overdrive, "overdrive", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, lid_switch, lang.muxtweakadv.lidswitch, "lidswitch", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, disp_suspend, lang.muxtweakadv.dispsuspend, "dispsuspend", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, stage_overlay, lang.muxtweakadv.stageoverlay, "stageoverlay", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, swapfile, lang.muxtweakadv.swapfile, "swapfile", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, zramfile, lang.muxtweakadv.zramfile, "zramfile", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, second_part, lang.muxtweakadv.secondpart, "secondpart", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, usb_part, lang.muxtweakadv.usbpart, "usbpart", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, inc_bright, lang.muxtweakadv.incbright, "incbright", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, inc_volume, lang.muxtweakadv.incvolume, "incvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, max_gpu, lang.muxtweakadv.maxgpu, "maxgpu", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, double_buffer, lang.muxtweakadv.doublebuffer, "doublebuffer", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, audio_ready, lang.muxtweakadv.audioready, "audioready", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, audio_swap, lang.muxtweakadv.audioswap, "audioswap", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, audio_suspend, lang.muxtweakadv.audiosuspend, "audiosuspend", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, bt_scan_timeout, lang.muxtweakadv.btscantimeout, "btscan", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, trust_modify, lang.muxtweakadv.trustmodify, "trustmodify", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, trust_power, lang.muxtweakadv.trustpower, "trustpower", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, trust_remove, lang.muxtweakadv.trustremove, "trustremove", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, usb_function, lang.muxtweakadv.usbfunction, "usbfunction", usb_functions, 3);
    INIT_OPTION_ITEM(-1, tweakadv, box_art_pad_div, lang.muxtweakadv.box_art_pad_div, "boxartpaddiv", NULL, 0);

    char pad_div_values[MAX_BUFFER_SIZE];
    snprintf(pad_div_values, sizeof(pad_div_values), "50\n100\n200\n400\n600\n800");
    apply_theme_list_drop_down(&theme, ui_dro_box_art_pad_div_tweakadv, pad_div_values);

    char *accelerate_values = generate_number_string(16, 256, 16, lang.generic.disabled, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_accelerate_tweakadv, accelerate_values);
    free(accelerate_values);

    char *repeat_delay_values = generate_number_string(16, 512, 16, lang.generic.disabled, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_repeat_delay_tweakadv, repeat_delay_values);
    free(repeat_delay_values);

    char *swap_values = generate_number_string(128, 1024, 128, lang.generic.disabled, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_swapfile_tweakadv, swap_values);
    apply_theme_list_drop_down(&theme, ui_dro_zramfile_tweakadv, swap_values);
    free(swap_values);

    char *partition_values = generate_number_string(1, 128, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_second_part_tweakadv, partition_values);
    apply_theme_list_drop_down(&theme, ui_dro_usb_part_tweakadv, partition_values);
    free(partition_values);

    char *increment_values = generate_number_string(1, 32, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_dro_inc_bright_tweakadv, increment_values);
    apply_theme_list_drop_down(&theme, ui_dro_inc_volume_tweakadv, increment_values);
    free(increment_values);

    char bt_scan_seconds[MAX_BUFFER_SIZE];
    snprintf(bt_scan_seconds, sizeof(bt_scan_seconds), " %s", lang.muxtweakadv.seconds);

    char *bt_scan_timeout_values = generate_number_string(5, 30, 5, NULL, bt_scan_seconds, NULL, 1);
    apply_theme_list_drop_down(&theme, ui_dro_bt_scan_timeout_tweakadv, bt_scan_timeout_values);
    free(bt_scan_timeout_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (!device.board.has_network) HIDE_OPTION_ITEM(tweakadv, retro_wait);
    if (!device.board.has_lid) HIDE_OPTION_ITEM(tweakadv, lid_switch);
    if (!device.board.has_stick) HIDE_OPTION_ITEM(tweakadv, stick_nav);
    if (!device.board.has_bluetooth) HIDE_OPTION_ITEM(tweakadv, bt_scan_timeout);

    // Removal of verbose messages due to changes to muterm not playing ball
    HIDE_OPTION_ITEM(tweakadv, verbose);

    // Hide specific items for the TrimUI devices
    if (str_startswith(device.board.name, "tui")) {
        HIDE_OPTION_ITEM(tweakadv, max_gpu);
    }
}

static void handle_option_prev(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;
    if (save_mode) {
        if (swap_axis) {
            dialogue_navigate(&save_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (hold_call) return;

    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_tweak_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

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

    if (!config.settings.advanced.trust_modify && any_tweakadv_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);
    save_tweak_options();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

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

#define TWEAKADV(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_tweakadv, UDATA);
    TWEAKADV_ELEMENTS
#undef TWEAKADV

    overlay_display();
}

int muxtweakadv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxtweakadv.title);
    init_muxtweakadv(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.back
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
            [mux_input_dpad_up] = handle_dpad_up,
            [mux_input_dpad_down] = handle_dpad_down,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
