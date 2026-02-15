#include "muxshare.h"
#include "ui/ui_muxwebserv.h"

#define UI_COUNT 5

#define WEBSERV(NAME, ENUM, UDATA) static int NAME##_original;
WEBSERV_ELEMENTS
#undef WEBSERV

static void show_help(void) {
    struct help_msg help_messages[] = {
#define WEBSERV(NAME, ENUM, UDATA) { UDATA, lang.MUXWEBSERV.HELP.ENUM },
            WEBSERV_ELEMENTS
#undef WEBSERV
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_dropdown_settings(void) {
#define WEBSERV(NAME, ENUM, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_webserv);
    WEBSERV_ELEMENTS
#undef WEBSERV
}

static void restore_web_options(void) {
#define WEBSERV(NAME, ENUM, UDATA) lv_dropdown_set_selected(ui_dro##NAME##_webserv, config.WEB.ENUM);
    WEBSERV_ELEMENTS
#undef WEBSERV
}

static void save_web_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(webserv, Sshd, "web/sshd", INT, 0);
    CHECK_AND_SAVE_STD(webserv, SftpGo, "web/sftpgo", INT, 0);
    CHECK_AND_SAVE_STD(webserv, Ttyd, "web/ttyd", INT, 0);
    CHECK_AND_SAVE_STD(webserv, Syncthing, "web/syncthing", INT, 0);
    CHECK_AND_SAVE_STD(webserv, Tailscaled, "web/tailscaled", INT, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);

        const char *args[] = {(OPT_PATH "script/web/service.sh"), NULL};
        run_exec(args, A_SIZE(args), 1, 0, NULL, NULL);

        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, webserv, Sshd, lang.MUXWEBSERV.SSHD, "sshd", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, SftpGo, lang.MUXWEBSERV.SFTPGO, "sftpgo", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, Ttyd, lang.MUXWEBSERV.TTYD, "ttyd", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, Syncthing, lang.MUXWEBSERV.SYNCTHING, "syncthing", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, webserv, Tailscaled, lang.MUXWEBSERV.TAILSCALED, "tailscaled", disabled_enabled, 2);

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, false);
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

    save_web_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "service");

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
            {ui_lblNavB,       lang.GENERIC.SAVE,   0},
            {NULL, NULL,                            0}
    });

#define WEBSERV(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_webserv, UDATA);
    WEBSERV_ELEMENTS
#undef WEBSERV

    overlay_display();
}

int muxwebserv_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXWEBSERV.TITLE);
    init_muxwebserv(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    restore_web_options();
    init_dropdown_settings();

    init_timer(ui_gen_refresh_task, NULL);
    list_nav_next(0);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_option_next,
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
