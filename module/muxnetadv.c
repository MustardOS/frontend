#include "muxshare.h"
#include "ui/ui_muxnetadv.h"

#define UI_COUNT 6

#define NETADV(NAME, UDATA) static int NAME##_original;
NETADV_ELEMENTS
#undef NETADV

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblMonitor_netadv, lang.MUXNETADV.HELP.MONITOR},
            {ui_lblBoot_netadv,    lang.MUXNETADV.HELP.BOOT},
            {ui_lblCompat_netadv,  lang.MUXNETADV.HELP.COMPAT},
            {ui_lblAsyncLoad_netadv, lang.MUXNETADV.HELP.ASYNCLOAD},
            {ui_lblWait_netadv,    lang.MUXNETADV.HELP.WAIT},
            {ui_lblRetry_netadv,   lang.MUXNETADV.HELP.RETRY},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define NETADV(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_netadv);
    NETADV_ELEMENTS
#undef NETADV
}

static void restore_netadv_options(void) {
    lv_dropdown_set_selected(ui_droMonitor_netadv, config.SETTINGS.NETWORK.MONITOR);
    lv_dropdown_set_selected(ui_droBoot_netadv, config.SETTINGS.NETWORK.BOOT);
    lv_dropdown_set_selected(ui_droCompat_netadv, config.SETTINGS.NETWORK.COMPAT);
    lv_dropdown_set_selected(ui_droAsyncLoad_netadv, config.SETTINGS.NETWORK.ASYNCLOAD);

    map_drop_down_to_index(ui_droWait_netadv, config.SETTINGS.NETWORK.WAIT, wait_retry_int, 9, 0);
    map_drop_down_to_index(ui_droRetry_netadv, config.SETTINGS.NETWORK.RETRY, wait_retry_int, 9, 0);
}

static void save_netadv_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(netadv, Monitor, "settings/network/monitor", INT, 0);
    CHECK_AND_SAVE_STD(netadv, Boot, "settings/network/boot", INT, 0);
    CHECK_AND_SAVE_STD(netadv, Compat, "settings/network/compat", INT, 0);
    CHECK_AND_SAVE_STD(netadv, AsyncLoad, "settings/network/async_load", INT, 0);

    CHECK_AND_SAVE_MAP(netadv, Wait, "settings/network/wait_timer", wait_retry_int, 9, 0);
    CHECK_AND_SAVE_MAP(netadv, Retry, "settings/network/compat_retry", wait_retry_int, 9, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);
        refresh_screen(ui_screen);

        refresh_config = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    char *wait_retry_str[] = {
            "1", "3", "5", "7", "10", "15", "20", "25", "30"
    };

    INIT_OPTION_ITEM(-1, netadv, Monitor, lang.MUXNETADV.MONITOR, "monitor", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Boot, lang.MUXNETADV.BOOT, "boot", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Compat, lang.MUXNETADV.COMPAT, "compat", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, AsyncLoad, lang.MUXNETADV.ASYNCLOAD, "asyncload", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, netadv, Wait, lang.MUXNETADV.WAIT, "wait", wait_retry_str, 9);
    INIT_OPTION_ITEM(-1, netadv, Retry, lang.MUXNETADV.RETRY, "retry", wait_retry_str, 9);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
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
    save_netadv_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "netadv");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help(lv_group_get_focused(ui_group));
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavLRGlyph, "",                  0},
            {ui_lblNavLR,      lang.GENERIC.CHANGE, 0},
            {ui_lblNavBGlyph,  "",                  0},
            {ui_lblNavB,       lang.GENERIC.BACK,   0},
            {NULL,             NULL,                0}
    });

#define NETADV(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_netadv, UDATA);
    NETADV_ELEMENTS
#undef NETADV

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxnetadv_main(void) {
    init_module("muxnetadv");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNETADV.TITLE);
    init_muxnetadv(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_netadv_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);
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
