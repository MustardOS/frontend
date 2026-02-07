#include "muxshare.h"
#include "ui/ui_muxtweakadv.h"

#define UI_COUNT 31

#define TWEAKADV(NAME, ENUM, UDATA) static int NAME##_original;
TWEAKADV_ELEMENTS
#undef TWEAKADV

static void show_help(void) {
    struct help_msg help_messages[] = {
#define TWEAKADV(NAME, ENUM, UDATA) { lang.MUXTWEAKADV.HELP.ENUM },
            TWEAKADV_ELEMENTS
#undef TWEAKADV
    };

    gen_help(current_item_index, UI_COUNT, help_messages, ui_group, items);
}

static void init_dropdown_settings(void) {
#define TWEAKADV(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_tweakadv);
    TWEAKADV_ELEMENTS
#undef TWEAKADV
}

static void restore_tweak_options(void) {
#define TWEAKADV(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_tweakadv, config.SETTINGS.ADVANCED.ENUM);
    TWEAKADV_ELEMENTS
#undef TWEAKADV

    lv_dropdown_set_selected(ui_droSecondPart_tweakadv, device.STORAGE.SDCARD.PARTITION - 1);
    lv_dropdown_set_selected(ui_droUsbPart_tweakadv, device.STORAGE.USB.PARTITION - 1);
    lv_dropdown_set_selected(ui_droIncBright_tweakadv, config.SETTINGS.ADVANCED.INCBRIGHT - 1);
    lv_dropdown_set_selected(ui_droIncVolume_tweakadv, config.SETTINGS.ADVANCED.INCVOLUME - 1);

    map_drop_down_to_index(ui_droOffset_tweakadv, config.SETTINGS.ADVANCED.OFFSET, battery_offset_values, 101, 50);
    map_drop_down_to_index(ui_droAccelerate_tweakadv, config.SETTINGS.ADVANCED.ACCELERATE, accelerate_values, 17, 6);
    map_drop_down_to_index(ui_droRepeatDelay_tweakadv, config.SETTINGS.ADVANCED.REPEATDELAY, repeat_delay_values, 33, 13);
    map_drop_down_to_index(ui_droSwapfile_tweakadv, config.SETTINGS.ADVANCED.SWAPFILE, swap_values, 11, 0);
    map_drop_down_to_index(ui_droZramfile_tweakadv, config.SETTINGS.ADVANCED.ZRAMFILE, swap_values, 11, 0);
}

static void save_tweak_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(tweakadv, Swap, "settings/advanced/swap", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, StickNav, "settings/advanced/sticknav", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Thermal, "settings/advanced/thermal", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Passcode, "settings/advanced/passcode", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Led, "settings/advanced/led", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, RandomTheme, "settings/advanced/random_theme", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, RetroWait, "settings/advanced/retrowait", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, RetroFree, "settings/advanced/retrofree", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, RetroCache, "settings/advanced/retrocache", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Activity, "settings/advanced/activity", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Verbose, "settings/advanced/verbose", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Rumble, "settings/advanced/rumble", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, UserInit, "settings/advanced/user_init", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, DpadSwap, "settings/advanced/dpad_swap", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, Overdrive, "settings/advanced/overdrive", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, LidSwitch, "settings/advanced/lidswitch", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, DispSuspend, "settings/advanced/disp_suspend", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, MaxGpu, "settings/advanced/maxgpu", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, AudioReady, "settings/advanced/audio_ready", INT, 0);
    CHECK_AND_SAVE_STD(tweakadv, AudioSwap, "settings/advanced/audio_swap", INT, 0);

    do {
        int sd2_current = lv_dropdown_get_selected(ui_droSecondPart_tweakadv);
        if (sd2_current != SecondPart_original) {
            is_modified++;
            write_text_to_file(CONF_DEVICE_PATH "storage/sdcard/num", "w", INT, sd2_current + 1);
        }
    } while (0);

    do {
        int usb_current = lv_dropdown_get_selected(ui_droUsbPart_tweakadv);
        if (usb_current != UsbPart_original) {
            is_modified++;
            write_text_to_file(CONF_DEVICE_PATH "storage/usb/num", "w", INT, usb_current + 1);
        }
    } while (0);

    do {
        int bright_current = lv_dropdown_get_selected(ui_droIncBright_tweakadv);
        if (bright_current != IncBright_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/incbright", "w", INT, bright_current + 1);
        }
    } while (0);

    do {
        int volume_current = lv_dropdown_get_selected(ui_droIncVolume_tweakadv);
        if (volume_current != IncVolume_original) {
            is_modified++;
            write_text_to_file(CONF_CONFIG_PATH "settings/advanced/incvolume", "w", INT, volume_current + 1);
        }
    } while (0);

    CHECK_AND_SAVE_VAL(tweakadv, Volume, "settings/advanced/volume", CHAR, volume_values);
    CHECK_AND_SAVE_VAL(tweakadv, Brightness, "settings/advanced/brightness", CHAR, brightness_values);

    CHECK_AND_SAVE_MAP(tweakadv, Accelerate, "settings/advanced/accelerate", accelerate_values, 17, 6);
    CHECK_AND_SAVE_MAP(tweakadv, Offset, "settings/advanced/offset", battery_offset_values, 101, 50);
    CHECK_AND_SAVE_MAP(tweakadv, RepeatDelay, "settings/advanced/repeat_delay", repeat_delay_values, 33, 13);
    CHECK_AND_SAVE_MAP(tweakadv, Swapfile, "settings/advanced/swapfile", swap_values, 11, 0);
    CHECK_AND_SAVE_MAP(tweakadv, Zramfile, "settings/advanced/zramfile", swap_values, 11, 0);

    if (is_modified > 0) run_tweak_script();
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *swap_options[] = {
            lang.MUXTWEAKADV.SWAP.RETRO,
            lang.MUXTWEAKADV.SWAP.MODERN
    };

    char *sticknav_options[] = {
            lang.MUXTWEAKADV.STICKNAV.DPAD,
            lang.MUXTWEAKADV.STICKNAV.LS,
            lang.MUXTWEAKADV.STICKNAV.RS,
            lang.MUXTWEAKADV.STICKNAV.DPAD_LS,
            lang.MUXTWEAKADV.STICKNAV.DPAD_RS,
            lang.MUXTWEAKADV.STICKNAV.DPAD_LS_RS,
            lang.MUXTWEAKADV.STICKNAV.LS_RS,
    };

    char *volume_options[] = {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.VOLUME.SILENT,
            lang.MUXTWEAKADV.VOLUME.SOFT,
            lang.MUXTWEAKADV.VOLUME.LOUD
    };

    char *brightness_options[] = {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.BRIGHTNESS.LOW,
            lang.MUXTWEAKADV.BRIGHTNESS.MEDIUM,
            lang.MUXTWEAKADV.BRIGHTNESS.HIGH
    };

    char *rumble_options[] = {
            lang.GENERIC.DISABLED,
            lang.MUXTWEAKADV.RUMBLE.ST,
            lang.MUXTWEAKADV.RUMBLE.SH,
            lang.MUXTWEAKADV.RUMBLE.SL,
            lang.MUXTWEAKADV.RUMBLE.STSH,
            lang.MUXTWEAKADV.RUMBLE.STSL,
            lang.MUXTWEAKADV.RUMBLE.SHSL
    };

    INIT_OPTION_ITEM(-1, tweakadv, Accelerate, lang.MUXTWEAKADV.ACCELERATE, "accelerate", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, RepeatDelay, lang.MUXTWEAKADV.REPEATDELAY, "repeat", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, Offset, lang.MUXTWEAKADV.OFFSET, "offset", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, Swap, lang.MUXTWEAKADV.SWAP.TITLE, "swap", swap_options, 2);
    INIT_OPTION_ITEM(-1, tweakadv, StickNav, lang.MUXTWEAKADV.STICKNAV.TITLE, "sticknav", sticknav_options, 7);
    INIT_OPTION_ITEM(-1, tweakadv, Volume, lang.MUXTWEAKADV.VOLUME.TITLE, "volume", volume_options, 4);
    INIT_OPTION_ITEM(-1, tweakadv, Brightness, lang.MUXTWEAKADV.BRIGHTNESS.TITLE, "brightness", brightness_options, 4);
    INIT_OPTION_ITEM(-1, tweakadv, Thermal, lang.MUXTWEAKADV.THERMAL, "thermal", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Passcode, lang.MUXTWEAKADV.PASSCODE, "passcode", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Led, lang.MUXTWEAKADV.LED, "led", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, RandomTheme, lang.MUXTWEAKADV.RANDOMTHEME, "randomtheme", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, RetroWait, lang.MUXTWEAKADV.RETROWAIT, "retrowait", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, RetroFree, lang.MUXTWEAKADV.RETROFREE, "retrofree", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, RetroCache, lang.MUXTWEAKADV.RETROCACHE, "retrocache", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Activity, lang.MUXTWEAKADV.ACTIVITY, "activity", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Verbose, lang.MUXTWEAKADV.VERBOSE, "verbose", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Rumble, lang.MUXTWEAKADV.RUMBLE.TITLE, "rumble", rumble_options, 7);
    INIT_OPTION_ITEM(-1, tweakadv, UserInit, lang.MUXTWEAKADV.USERINIT, "userinit", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, DpadSwap, lang.MUXTWEAKADV.DPADSWAP, "dpadswap", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Overdrive, lang.MUXTWEAKADV.OVERDRIVE, "overdrive", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, LidSwitch, lang.MUXTWEAKADV.LIDSWITCH, "lidswitch", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, DispSuspend, lang.MUXTWEAKADV.DISPSUSPEND, "dispsuspend", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, Swapfile, lang.MUXTWEAKADV.SWAPFILE, "swapfile", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, Zramfile, lang.MUXTWEAKADV.ZRAMFILE, "zramfile", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, SecondPart, lang.MUXTWEAKADV.SECONDPART, "secondpart", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, UsbPart, lang.MUXTWEAKADV.USBPART, "usbpart", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, IncBright, lang.MUXTWEAKADV.INCBRIGHT, "incbright", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, IncVolume, lang.MUXTWEAKADV.INCVOLUME, "incvolume", NULL, 0);
    INIT_OPTION_ITEM(-1, tweakadv, MaxGpu, lang.MUXTWEAKADV.MAXGPU, "maxgpu", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, AudioReady, lang.MUXTWEAKADV.AUDIOREADY, "audioready", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, tweakadv, AudioSwap, lang.MUXTWEAKADV.AUDIOSWAP, "audioswap", disabled_enabled, 2);

    char *accelerate_values = generate_number_string(16, 256, 16, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droAccelerate_tweakadv, accelerate_values);
    free(accelerate_values);

    char *repeat_delay_values = generate_number_string(16, 512, 16, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droRepeatDelay_tweakadv, repeat_delay_values);
    free(repeat_delay_values);

    char *offset_values = generate_number_string(-50, 50, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOffset_tweakadv, offset_values);
    free(offset_values);

    char *swapfile_values = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSwapfile_tweakadv, swapfile_values);
    free(swapfile_values);

    char *zramfile_values = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droZramfile_tweakadv, zramfile_values);
    free(zramfile_values);

    char *partition_values = generate_number_string(1, 128, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSecondPart_tweakadv, partition_values);
    apply_theme_list_drop_down(&theme, ui_droUsbPart_tweakadv, partition_values);
    free(partition_values);

    char *increment_values = generate_number_string(1, 16, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droIncBright_tweakadv, increment_values);
    apply_theme_list_drop_down(&theme, ui_droIncVolume_tweakadv, increment_values);
    free(increment_values);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);

    if (!device.BOARD.HASNETWORK) HIDE_OPTION_ITEM(tweakadv, RetroWait);
    if (!device.BOARD.HASLID) HIDE_OPTION_ITEM(tweakadv, LidSwitch);
    if (!device.BOARD.HASSTICK) HIDE_OPTION_ITEM(tweakadv, StickNav);

    // Removal of verbose messages due to changes to muterm not playing ball
    HIDE_OPTION_ITEM(tweakadv, Verbose);

    // Hide specific items for the TrimUI devices
    if (str_startswith(device.BOARD.NAME, "tui")) {
        HIDE_OPTION_ITEM(tweakadv, Zramfile);
        HIDE_OPTION_ITEM(tweakadv, MaxGpu);
    }
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

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    handle_option_next();
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

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

    close_input();
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
            {NULL, NULL,                            0}
    });

#define TWEAKADV(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_tweakadv, UDATA);
    TWEAKADV_ELEMENTS
#undef TWEAKADV

    overlay_display();
}

int muxtweakadv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKADV.TITLE);
    init_muxtweakadv(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

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
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
