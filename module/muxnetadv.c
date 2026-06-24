#include "muxshare.h"
#include "ui/ui_muxnetadv.h"

#define NETADV(NAME, ENUM, UDATA) 1,
enum {
    UI_COUNT = E_SIZE(NETADV_ELEMENTS)
};
#undef NETADV

#define NETADV(NAME, ENUM, UDATA) static int NAME##_original;
NETADV_ELEMENTS
#undef NETADV

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

static int any_netadv_modified(void) {
#define NETADV(NAME, ENUM, UDATA) if ((int) lv_dropdown_get_selected(ui_dro##NAME##_netadv) != NAME##_original) return 1;
    NETADV_ELEMENTS
#undef NETADV
    return 0;
}

static void show_help(void) {
    struct help_msg help_messages[] = {
#define NETADV(NAME, ENUM, UDATA) { UDATA, lang.MUXNETADV.HELP.ENUM },
            NETADV_ELEMENTS
#undef NETADV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define NETADV(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_netadv);
    NETADV_ELEMENTS
#undef NETADV
}

static void restore_netadv_options(void) {
#define NETADV(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_netadv, config.SETTINGS.NETWORK.ENUM);
    NETADV_ELEMENTS
#undef NETADV

    map_drop_down_to_index(ui_droConRetry_netadv, config.SETTINGS.NETWORK.CONRETRY, wait_retry_int, 9, 0);
    map_drop_down_to_index(ui_droWait_netadv, config.SETTINGS.NETWORK.WAIT, wait_retry_int, 9, 0);
    map_drop_down_to_index(ui_droModRetry_netadv, config.SETTINGS.NETWORK.MODRETRY, wait_retry_int, 9, 0);
}

static void save_netadv_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(netadv, Monitor, "settings/network/monitor", INT, 0);
    CHECK_AND_SAVE_STD(netadv, Boot, "settings/network/boot", INT, 0);
    CHECK_AND_SAVE_STD(netadv, Wake, "settings/network/wake", INT, 0);
    CHECK_AND_SAVE_STD(netadv, Compat, "settings/network/compat", INT, 0);
    CHECK_AND_SAVE_STD(netadv, AsyncLoad, "settings/network/async_load", INT, 0);
    CHECK_AND_SAVE_MAP(netadv, ConRetry, "settings/network/con_retry", wait_retry_int, 9, 0);

    CHECK_AND_SAVE_MAP(netadv, Wait, "settings/network/wait_timer", wait_retry_int, 9, 0);
    CHECK_AND_SAVE_MAP(netadv, ModRetry, "settings/network/mod_retry", wait_retry_int, 9, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, netadv, Monitor, lang.MUXNETADV.MONITOR, "monitor", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Boot, lang.MUXNETADV.BOOT, "boot", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Wake, lang.MUXNETADV.WAKE, "wake", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Compat, lang.MUXNETADV.COMPAT, "compat", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, AsyncLoad, lang.MUXNETADV.ASYNCLOAD, "asyncload", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, ConRetry, lang.MUXNETADV.CONRETRY, "conretry", wait_retry_str, 9);
    INIT_OPTION_ITEM(-1, netadv, Wait, lang.MUXNETADV.WAIT, "wait", wait_retry_str, 9);
    INIT_OPTION_ITEM(-1, netadv, ModRetry, lang.MUXNETADV.MODRETRY, "modretry", wait_retry_str, 9);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
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
}

static void handle_a(void) {
    if (save_mode) {
        mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == MUX_UNSAVED_SAVE) save_netadv_options();

        play_sound(opt == MUX_UNSAVED_SAVE ? SND_CONFIRM : SND_BACK);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netadv");

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

    if (!config.SETTINGS.ADVANCED.TRUSTMODIFY && any_netadv_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(SND_BACK);
    save_netadv_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netadv");

    mux_input_stop();
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
}

static void handle_dpad_up_hold(void) {
    if (save_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (save_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call || save_mode) return;

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

#define NETADV(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_netadv, UDATA);
    NETADV_ELEMENTS
#undef NETADV

    overlay_display();
}

int muxnetadv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETADV.TITLE);
    init_muxnetadv(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_netadv_options();
    init_dropdown_settings();

    dialogue_init_unsaved(&save_dlg, &theme, ui_screen, lang.GENERIC.UNSAVED, NULL,
                          lang.GENERIC.SAVE, lang.GENERIC.DISCARD, lang.GENERIC.SELECT, lang.GENERIC.BACK);
    init_timer(ui_gen_refresh_task, NULL);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
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

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
