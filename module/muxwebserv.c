#include "muxshare.h"
#include "ui/ui_muxwebserv.h"

#define WEBSERV(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(WEBSERV_ELEMENTS) };
#undef WEBSERV

#define WEBSERV(NAME, UDATA) static int NAME##_original;
WEBSERV_ELEMENTS
#undef WEBSERV

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

static int any_web_modified(void) {
#define WEBSERV(NAME, UDATA)                                                                                           \
    if ((int) lv_dropdown_get_selected(ui_dro_##NAME##_webserv) != NAME##_original) return 1;
    WEBSERV_ELEMENTS
#undef WEBSERV
    return 0;
}

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define WEBSERV(NAME, UDATA) {UDATA, lang.muxwebserv.help.NAME},
        WEBSERV_ELEMENTS
#undef WEBSERV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define WEBSERV(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro_##NAME##_webserv);
    WEBSERV_ELEMENTS
#undef WEBSERV
}

static void restore_web_options(void) {
#define WEBSERV(NAME, UDATA) lv_dropdown_set_selected(ui_dro_##NAME##_webserv, config.web.NAME);
    WEBSERV_ELEMENTS
#undef WEBSERV
}

static void save_web_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(webserv, sshd, "web/sshd", INT, 0);
    CHECK_AND_SAVE_STD(webserv, sftp_go, "web/sftpgo", INT, 0);
    CHECK_AND_SAVE_STD(webserv, ttyd, "web/ttyd", INT, 0);
    CHECK_AND_SAVE_STD(webserv, syncthing, "web/syncthing", INT, 0);
    CHECK_AND_SAVE_STD(webserv, tailscaled, "web/tailscaled", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.generic.saving, tst_wait_f);

        const char *args[] = {OPT_PATH "script/web/service.sh", NULL};
        run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);

        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_OPTION_ITEM(-1, webserv, sshd, lang.muxwebserv.sshd, "sshd", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, sftp_go, lang.muxwebserv.sftpgo, "sftpgo", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, ttyd, lang.muxwebserv.ttyd, "ttyd", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, syncthing, lang.muxwebserv.syncthing, "syncthing", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, tailscaled, lang.muxwebserv.tailscaled, "tailscaled", disabled_enabled, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);
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
}

static void handle_a(void) {
    if (save_mode) {
        const mux_unsaved_opt opt = (mux_unsaved_opt) save_dlg.selected;
        hide_save_dialog();

        if (opt == mux_unsaved_save) save_web_options();

        play_sound(opt == mux_unsaved_save ? snd_confirm : snd_back);
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "service");

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

    if (!config.settings.advanced.trust_modify && any_web_modified()) {
        show_save_dialog();
        return;
    }

    play_sound(snd_back);
    save_web_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "service");

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

#define WEBSERV(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_webserv, UDATA);
    WEBSERV_ELEMENTS
#undef WEBSERV

    overlay_display();
}

int muxwebserv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxwebserv.title);
    init_muxwebserv(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_navigation_group();

    restore_web_options();
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
