#include "muxshare.h"
#include "ui/ui_muxnetadv.h"

#define NETADV(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(NETADV_ELEMENTS) };
#undef NETADV

#define NETADV(NAME, UDATA) static int NAME##_original;
NETADV_ELEMENTS
#undef NETADV

static int save_mode = 0;
static mux_dialogue save_dlg;

static void hide_save_dialog(void) {
    dialogue_dismiss(&save_mode, &save_dlg);
}

static int any_netadv_modified(void) {
#define NETADV(NAME, UDATA)                                                                                            \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_netadv) != NAME##_original) return 1;
    NETADV_ELEMENTS
#undef NETADV
    return 0;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define NETADV(NAME, UDATA) {UDATA, lang.muxnetadv.help.NAME},
        NETADV_ELEMENTS
#undef NETADV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define NETADV(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_netadv);
    NETADV_ELEMENTS
#undef NETADV
}

static void restore_netadv_options(void) {
#define NETADV(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_netadv, config.settings.network.NAME);
    NETADV_ELEMENTS
#undef NETADV

    map_drop_down_to_index(ui_dro_con_retry_netadv, config.settings.network.con_retry, wait_retry_int, 9, 0);
    map_drop_down_to_index(ui_dro_wait_netadv, config.settings.network.wait, wait_retry_int, 9, 0);
    map_drop_down_to_index(ui_dro_mod_retry_netadv, config.settings.network.mod_retry, wait_retry_int, 9, 0);
}

static void save_netadv_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(netadv, system_dns, "settings/network/system_dns", INT, 0);
    CHECK_AND_SAVE_STD(netadv, monitor, "settings/network/monitor", INT, 0);
    CHECK_AND_SAVE_STD(netadv, boot, "settings/network/boot", INT, 0);
    CHECK_AND_SAVE_STD(netadv, wake, "settings/network/wake", INT, 0);
    CHECK_AND_SAVE_STD(netadv, compat, "settings/network/compat", INT, 0);
    CHECK_AND_SAVE_STD(netadv, async_load, "settings/network/async_load", INT, 0);
    CHECK_AND_SAVE_MAP(netadv, con_retry, "settings/network/con_retry", wait_retry_int, 9, 0);

    CHECK_AND_SAVE_MAP(netadv, wait, "settings/network/wait_timer", wait_retry_int, 9, 0);
    CHECK_AND_SAVE_MAP(netadv, mod_retry, "settings/network/mod_retry", wait_retry_int, 9, 0);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);
        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *dns_options[] = {lang.muxnetadv.dns_name.def, lang.muxnetadv.dns_name.cloudflare,
                           lang.muxnetadv.dns_name.google, lang.muxnetadv.dns_name.quad9};

    INIT_OPTION_ITEM(-1, netadv, system_dns, lang.muxnetadv.system_dns, "systemdns", dns_options, 4);
    INIT_OPTION_ITEM(-1, netadv, monitor, lang.muxnetadv.monitor, "monitor", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, boot, lang.muxnetadv.boot, "boot", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, wake, lang.muxnetadv.wake, "wake", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, compat, lang.muxnetadv.compat, "compat", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, async_load, lang.muxnetadv.asyncload, "asyncload", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, con_retry, lang.muxnetadv.conretry, "conretry", wait_retry_str, 9);
    INIT_OPTION_ITEM(-1, netadv, wait, lang.muxnetadv.wait, "wait", wait_retry_str, 9);
    INIT_OPTION_ITEM(-1, netadv, mod_retry, lang.muxnetadv.modretry, "modretry", wait_retry_str, 9);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
}

static void handle_option_prev(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), -1);
}

static void handle_option_next(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    move_option(lv_group_get_focused(ui_group_value), +1);
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_netadv_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
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

    if (dialogue_guard_unsaved(&save_mode, &save_dlg, &theme, any_netadv_modified())) return;

    play_sound(snd_back);
    save_netadv_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netadv");

    mux_input_stop();
}

static void handle_dpad_up(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (save_mode) {
        dialogue_handle_dpad(&save_dlg, &theme, +1, !swap_axis);
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

#define NETADV(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_netadv, UDATA);
    NETADV_ELEMENTS
#undef NETADV

    overlay_display();
}

int muxnetadv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxnetadv.title);
    init_muxnetadv(ui_pnl_content);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_netadv_options();
    init_dropdown_settings();

    dialogue_init_unsaved(
        &save_dlg, &theme, ui_screen, lang.generic.unsaved, NULL, lang.generic.save, lang.generic.discard,
        lang.generic.select, lang.generic.cancel
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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
