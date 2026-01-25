#include "muxshare.h"
#include "ui/ui_muxinfo.h"

#define UI_COUNT 9

static void list_nav_move(int steps, int direction);

static void show_help() {
    struct help_msg help_messages[] = {
#define INFO(NAME, ENUM, UDATA) { ui_lbl##NAME##_info, lang.MUXINFO.HELP.ENUM },
            INFO_ELEMENTS
#undef INFO
    };

    gen_help(lv_group_get_focused(ui_group), help_messages, A_SIZE(help_messages));
}

static int visible_network_opt(void) {
    return device.BOARD.HASNETWORK;
}

static int visible_chrony_opt(void) {
    return 0;
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_STATIC_ITEM(-1, info, News, lang.MUXINFO.NEWS, "news", 0);
    INIT_STATIC_ITEM(-1, info, Activity, lang.MUXINFO.ACTIVITY, "activity", 0);
    INIT_STATIC_ITEM(-1, info, Screenshot, lang.MUXINFO.SCREENSHOT, "screenshot", 0);
    INIT_STATIC_ITEM(-1, info, Space, lang.MUXINFO.SPACE, "space", 0);
    INIT_STATIC_ITEM(-1, info, Tester, lang.MUXINFO.TESTER, "tester", 0);
    INIT_STATIC_ITEM(-1, info, SysInfo, lang.MUXINFO.SYSINFO, "sysinfo", 0);
    INIT_STATIC_ITEM(-1, info, NetInfo, lang.MUXINFO.NETINFO, "netinfo", 0);
    INIT_STATIC_ITEM(-1, info, Chrony, lang.MUXINFO.CHRONY, "chrony", 0);
    INIT_STATIC_ITEM(-1, info, Credit, lang.MUXINFO.CREDIT, "credit", 0);

    reset_ui_groups();
    add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, false);

    if (!visible_network_opt()) {
        HIDE_STATIC_ITEM(info, News);
        HIDE_STATIC_ITEM(info, NetInfo);
    }

    // Hide until further notice or future development
    if (!visible_chrony_opt()) HIDE_STATIC_ITEM(info, Chrony);

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (msgbox_active || hold_call) return;

    typedef enum {
        MENU_GENERAL = 0,
        MENU_NEWS,
    } menu_action;

    typedef int (*visible_fn)(void);

    typedef struct {
        const char *mux_name;
        menu_action action;
        visible_fn visible;
    } menu_entry;

    static const menu_entry entries[UI_COUNT] = {
            {"news",       MENU_NEWS,    visible_network_opt},
            {"activity",   MENU_GENERAL, NULL},
            {"screenshot", MENU_GENERAL, NULL},
            {"space",      MENU_GENERAL, NULL},
            {"tester",     MENU_GENERAL, NULL},
            {"sysinfo",    MENU_GENERAL, NULL},
            {"netinfo",    MENU_GENERAL, visible_network_opt},
            {"chrony",     MENU_GENERAL, visible_chrony_opt},
            {"credits",    MENU_GENERAL, NULL},
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
        case MENU_GENERAL:
            play_sound(SND_CONFIRM);
            load_mux(entry->mux_name);

            close_input();
            mux_input_stop();
            break;
        case MENU_NEWS:
            if (is_network_connected()) {
                play_sound(SND_CONFIRM);
                load_mux(entry->mux_name);

                close_input();
                mux_input_stop();
            } else {
                play_sound(SND_ERROR);
                toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
                return;
            }
            break;
        default:
            return;
    }
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
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "info");

    close_input();
    mux_input_stop();
}

static void handle_menu(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                  0},
            {ui_lblNavA,      lang.GENERIC.SELECT, 0},
            {ui_lblNavBGlyph, "",                  0},
            {ui_lblNavB,      lang.GENERIC.BACK,   0},
            {NULL, NULL,                           0}
    });

#define INFO(NAME, ENUM, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_info, UDATA);
    INFO_ELEMENTS
#undef INFO

    overlay_display();
}

int muxinfo_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXINFO.TITLE);
    init_muxinfo(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, WALL_GENERAL);

    init_fonts();
    init_navigation_group();

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
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
