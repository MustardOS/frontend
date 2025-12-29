#include "muxshare.h"
#include "ui/ui_muxkiosk.h"

#define UI_COUNT 40

#define KIOSK(NAME, UDATA) static int NAME##_original;
KIOSK_ELEMENTS
#undef KIOSK

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblEnable_kiosk,     lang.MUXKIOSK.HELP.ENABLE},
            {ui_lblMessage_kiosk,    lang.MUXKIOSK.HELP.MESSAGE},
            {ui_lblArchive_kiosk,    lang.MUXKIOSK.HELP.ARCHIVE},
            {ui_lblTask_kiosk,       lang.MUXKIOSK.HELP.TASK},
            {ui_lblCustom_kiosk,     lang.MUXKIOSK.HELP.CUSTOM},
            {ui_lblLanguage_kiosk,   lang.MUXKIOSK.HELP.LANGUAGE},
            {ui_lblNetwork_kiosk,    lang.MUXKIOSK.HELP.NETWORK},
            {ui_lblStorage_kiosk,    lang.MUXKIOSK.HELP.STORAGE},
            {ui_lblBackup_kiosk,     lang.MUXKIOSK.HELP.BACKUP},
            {ui_lblNetAdv_kiosk,     lang.MUXKIOSK.HELP.NETADV},
            {ui_lblWebServ_kiosk,    lang.MUXKIOSK.HELP.WEBSERV},
            {ui_lblCore_kiosk,       lang.MUXKIOSK.HELP.CORE},
            {ui_lblGovernor_kiosk,   lang.MUXKIOSK.HELP.GOVERNOR},
            {ui_lblControl_kiosk,    lang.MUXKIOSK.HELP.CONTROL},
            {ui_lblOption_kiosk,     lang.MUXKIOSK.HELP.OPTION},
            {ui_lblRetroArch_kiosk,  lang.MUXKIOSK.HELP.RETROARCH},
            {ui_lblSearch_kiosk,     lang.MUXKIOSK.HELP.SEARCH},
            {ui_lblTag_kiosk,        lang.MUXKIOSK.HELP.TAG},
            {ui_lblCatalogue_kiosk,  lang.MUXKIOSK.HELP.CATALOGUE},
            {ui_lblRAConfig_kiosk,   lang.MUXKIOSK.HELP.RACONFIG},
            {ui_lblTheme_kiosk,      lang.MUXKIOSK.HELP.THEME},
            {ui_lblThemeDown_kiosk,  lang.MUXKIOSK.HELP.THEME_DOWN},
            {ui_lblClock_kiosk,      lang.MUXKIOSK.HELP.CLOCK},
            {ui_lblTimezone_kiosk,   lang.MUXKIOSK.HELP.TIMEZONE},
            {ui_lblApps_kiosk,       lang.MUXKIOSK.HELP.APPS},
            {ui_lblConfig_kiosk,     lang.MUXKIOSK.HELP.CONFIG},
            {ui_lblExplore_kiosk,    lang.MUXKIOSK.HELP.EXPLORE},
            {ui_lblCollectMod_kiosk, lang.MUXKIOSK.HELP.COLLECTION.MAIN},
            {ui_lblCollectAdd_kiosk, lang.MUXKIOSK.HELP.COLLECTION.ADD_CONTENT},
            {ui_lblCollectNew_kiosk, lang.MUXKIOSK.HELP.COLLECTION.NEW_DIR},
            {ui_lblCollectRem_kiosk, lang.MUXKIOSK.HELP.COLLECTION.REMOVE},
            {ui_lblCollectAcc_kiosk, lang.MUXKIOSK.HELP.COLLECTION.ACCESS},
            {ui_lblHistoryMod_kiosk, lang.MUXKIOSK.HELP.HISTORY.MAIN},
            {ui_lblHistoryRem_kiosk, lang.MUXKIOSK.HELP.HISTORY.REMOVE},
            {ui_lblInfo_kiosk,       lang.MUXKIOSK.HELP.INFO},
            {ui_lblAdvanced_kiosk,   lang.MUXKIOSK.HELP.ADVANCED},
            {ui_lblGeneral_kiosk,    lang.MUXKIOSK.HELP.GENERAL},
            {ui_lblHdmi_kiosk,       lang.MUXKIOSK.HELP.HDMI},
            {ui_lblPower_kiosk,      lang.MUXKIOSK.HELP.POWER},
            {ui_lblVisual_kiosk,     lang.MUXKIOSK.HELP.VISUAL},
    };

    gen_help(element_focused, help_messages, A_SIZE(help_messages));
}

static void init_dropdown_settings(void) {
#define KIOSK(NAME, UDATA) NAME##_original = lv_dropdown_get_selected(ui_dro##NAME##_kiosk);
    KIOSK_ELEMENTS
#undef KIOSK
}

static void restore_kiosk_options(void) {
    lv_dropdown_set_selected(ui_droEnable_kiosk, kiosk.ENABLE);
    lv_dropdown_set_selected(ui_droMessage_kiosk, kiosk.MESSAGE);
    lv_dropdown_set_selected(ui_droArchive_kiosk, kiosk.APPLICATION.ARCHIVE);
    lv_dropdown_set_selected(ui_droTask_kiosk, kiosk.APPLICATION.TASK);
    lv_dropdown_set_selected(ui_droCustom_kiosk, kiosk.CONFIG.CUSTOMISATION);
    lv_dropdown_set_selected(ui_droLanguage_kiosk, kiosk.CONFIG.LANGUAGE);
    lv_dropdown_set_selected(ui_droNetwork_kiosk, kiosk.CONFIG.NETWORK);
    lv_dropdown_set_selected(ui_droStorage_kiosk, kiosk.CONFIG.STORAGE);
    lv_dropdown_set_selected(ui_droBackup_kiosk, kiosk.CONFIG.BACKUP);
    lv_dropdown_set_selected(ui_droNetAdv_kiosk, kiosk.CONFIG.NET_SETTINGS);
    lv_dropdown_set_selected(ui_droWebServ_kiosk, kiosk.CONFIG.WEB_SERVICES);
    lv_dropdown_set_selected(ui_droCore_kiosk, kiosk.CONTENT.CORE);
    lv_dropdown_set_selected(ui_droGovernor_kiosk, kiosk.CONTENT.GOVERNOR);
    lv_dropdown_set_selected(ui_droControl_kiosk, kiosk.CONTENT.CONTROL);
    lv_dropdown_set_selected(ui_droOption_kiosk, kiosk.CONTENT.OPTION);
    lv_dropdown_set_selected(ui_droRetroArch_kiosk, kiosk.CONTENT.RETROARCH);
    lv_dropdown_set_selected(ui_droSearch_kiosk, kiosk.CONTENT.SEARCH);
    lv_dropdown_set_selected(ui_droTag_kiosk, kiosk.CONTENT.TAG);
    lv_dropdown_set_selected(ui_droCatalogue_kiosk, kiosk.CUSTOM.CATALOGUE);
    lv_dropdown_set_selected(ui_droRAConfig_kiosk, kiosk.CUSTOM.RACONFIG);
    lv_dropdown_set_selected(ui_droTheme_kiosk, kiosk.CUSTOM.THEME);
    lv_dropdown_set_selected(ui_droThemeDown_kiosk, kiosk.CUSTOM.THEME_DOWN);
    lv_dropdown_set_selected(ui_droClock_kiosk, kiosk.DATETIME.CLOCK);
    lv_dropdown_set_selected(ui_droTimezone_kiosk, kiosk.DATETIME.TIMEZONE);
    lv_dropdown_set_selected(ui_droApps_kiosk, kiosk.LAUNCH.APPLICATION);
    lv_dropdown_set_selected(ui_droConfig_kiosk, kiosk.LAUNCH.CONFIGURATION);
    lv_dropdown_set_selected(ui_droExplore_kiosk, kiosk.LAUNCH.EXPLORE);
    lv_dropdown_set_selected(ui_droCollectMod_kiosk, kiosk.LAUNCH.COLLECTION);
    lv_dropdown_set_selected(ui_droCollectAdd_kiosk, kiosk.COLLECT.ADD_CON);
    lv_dropdown_set_selected(ui_droCollectNew_kiosk, kiosk.COLLECT.NEW_DIR);
    lv_dropdown_set_selected(ui_droCollectRem_kiosk, kiosk.COLLECT.REMOVE);
    lv_dropdown_set_selected(ui_droCollectAcc_kiosk, kiosk.COLLECT.ACCESS);
    lv_dropdown_set_selected(ui_droHistoryMod_kiosk, kiosk.LAUNCH.HISTORY);
    lv_dropdown_set_selected(ui_droHistoryRem_kiosk, kiosk.CONTENT.HISTORY);
    lv_dropdown_set_selected(ui_droInfo_kiosk, kiosk.LAUNCH.INFORMATION);
    lv_dropdown_set_selected(ui_droAdvanced_kiosk, kiosk.SETTING.ADVANCED);
    lv_dropdown_set_selected(ui_droGeneral_kiosk, kiosk.SETTING.GENERAL);
    lv_dropdown_set_selected(ui_droHdmi_kiosk, kiosk.SETTING.HDMI);
    lv_dropdown_set_selected(ui_droPower_kiosk, kiosk.SETTING.POWER);
    lv_dropdown_set_selected(ui_droVisual_kiosk, kiosk.SETTING.VISUAL);
}

static void save_kiosk_options(void) {
    int is_modified = 0;

    CHECK_AND_SAVE_KSK(kiosk, Enable, "enable", INT);
    CHECK_AND_SAVE_KSK(kiosk, Message, "message", INT);
    CHECK_AND_SAVE_KSK(kiosk, Archive, "application/archive", INT);
    CHECK_AND_SAVE_KSK(kiosk, Task, "application/task", INT);
    CHECK_AND_SAVE_KSK(kiosk, Custom, "config/custom", INT);
    CHECK_AND_SAVE_KSK(kiosk, Language, "config/language", INT);
    CHECK_AND_SAVE_KSK(kiosk, Network, "config/network", INT);
    CHECK_AND_SAVE_KSK(kiosk, Storage, "config/storage", INT);
    CHECK_AND_SAVE_KSK(kiosk, Backup, "config/backup", INT);
    CHECK_AND_SAVE_KSK(kiosk, NetAdv, "config/netadv", INT);
    CHECK_AND_SAVE_KSK(kiosk, WebServ, "config/webserv", INT);
    CHECK_AND_SAVE_KSK(kiosk, Core, "content/core", INT);
    CHECK_AND_SAVE_KSK(kiosk, Governor, "content/governor", INT);
    CHECK_AND_SAVE_KSK(kiosk, Control, "content/control", INT);
    CHECK_AND_SAVE_KSK(kiosk, Tag, "content/tag", INT);
    CHECK_AND_SAVE_KSK(kiosk, Option, "content/option", INT);
    CHECK_AND_SAVE_KSK(kiosk, RetroArch, "content/retroarch", INT);
    CHECK_AND_SAVE_KSK(kiosk, Search, "content/search", INT);
    CHECK_AND_SAVE_KSK(kiosk, HistoryRem, "content/history", INT);
    CHECK_AND_SAVE_KSK(kiosk, CollectAdd, "collect/add_con", INT);
    CHECK_AND_SAVE_KSK(kiosk, CollectNew, "collect/new_dir", INT);
    CHECK_AND_SAVE_KSK(kiosk, CollectRem, "collect/remove", INT);
    CHECK_AND_SAVE_KSK(kiosk, CollectAcc, "collect/access", INT);
    CHECK_AND_SAVE_KSK(kiosk, Catalogue, "custom/catalogue", INT);
    CHECK_AND_SAVE_KSK(kiosk, RAConfig, "custom/raconfig", INT);
    CHECK_AND_SAVE_KSK(kiosk, Theme, "custom/theme", INT);
    CHECK_AND_SAVE_KSK(kiosk, ThemeDown, "custom/theme_down", INT);
    CHECK_AND_SAVE_KSK(kiosk, Clock, "datetime/clock", INT);
    CHECK_AND_SAVE_KSK(kiosk, Timezone, "datetime/timezone", INT);
    CHECK_AND_SAVE_KSK(kiosk, Apps, "launch/apps", INT);
    CHECK_AND_SAVE_KSK(kiosk, Config, "launch/config", INT);
    CHECK_AND_SAVE_KSK(kiosk, Explore, "launch/explore", INT);
    CHECK_AND_SAVE_KSK(kiosk, CollectMod, "launch/collection", INT);
    CHECK_AND_SAVE_KSK(kiosk, HistoryMod, "launch/history", INT);
    CHECK_AND_SAVE_KSK(kiosk, Info, "launch/info", INT);
    CHECK_AND_SAVE_KSK(kiosk, Advanced, "setting/advanced", INT);
    CHECK_AND_SAVE_KSK(kiosk, General, "setting/general", INT);
    CHECK_AND_SAVE_KSK(kiosk, Hdmi, "setting/hdmi", INT);
    CHECK_AND_SAVE_KSK(kiosk, Power, "setting/power", INT);
    CHECK_AND_SAVE_KSK(kiosk, Visual, "setting/visual", INT);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, FOREVER);

        if (file_exist(COLLECTION_DIR)) remove(COLLECTION_DIR);
        if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);

        refresh_screen(ui_screen);
        refresh_kiosk = 1;
    }
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    INIT_OPTION_ITEM(-1, kiosk, Enable, lang.MUXKIOSK.ENABLE, "enable", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, kiosk, Message, lang.MUXKIOSK.MESSAGE, "message", disabled_enabled, 2);
    INIT_OPTION_ITEM(-1, kiosk, Archive, lang.MUXKIOSK.ARCHIVE, "archive", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Task, lang.MUXKIOSK.TASK, "task", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Custom, lang.MUXKIOSK.CUSTOM, "custom", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Language, lang.MUXKIOSK.LANGUAGE, "language", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Network, lang.MUXKIOSK.NETWORK, "network", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Storage, lang.MUXKIOSK.STORAGE, "storage", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Backup, lang.MUXKIOSK.BACKUP, "backup", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, NetAdv, lang.MUXKIOSK.NETADV, "netadv", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, WebServ, lang.MUXKIOSK.WEBSERV, "webserv", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Core, lang.MUXKIOSK.CORE, "core", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Governor, lang.MUXKIOSK.GOVERNOR, "governor", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Control, lang.MUXKIOSK.CONTROL, "control", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Option, lang.MUXKIOSK.OPTION, "option", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, RetroArch, lang.MUXKIOSK.RETROARCH, "retroarch", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Search, lang.MUXKIOSK.SEARCH, "search", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Tag, lang.MUXKIOSK.TAG, "tag", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Catalogue, lang.MUXKIOSK.CATALOGUE, "catalogue", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, RAConfig, lang.MUXKIOSK.RACONFIG, "raconfig", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Theme, lang.MUXKIOSK.THEME, "theme", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, ThemeDown, lang.MUXKIOSK.THEME_DOWN, "theme_down", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Clock, lang.MUXKIOSK.CLOCK, "clock", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Timezone, lang.MUXKIOSK.TIMEZONE, "timezone", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Apps, lang.MUXKIOSK.APPS, "apps", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Config, lang.MUXKIOSK.CONFIG, "config", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Explore, lang.MUXKIOSK.EXPLORE, "explore", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, CollectMod, lang.MUXKIOSK.COLLECTION.MAIN, "collectmod", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, CollectAdd, lang.MUXKIOSK.COLLECTION.ADD_CONTENT, "collectadd", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, CollectNew, lang.MUXKIOSK.COLLECTION.NEW_DIR, "collectnew", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, CollectRem, lang.MUXKIOSK.COLLECTION.REMOVE, "collectrem", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, CollectAcc, lang.MUXKIOSK.COLLECTION.ACCESS, "collectacc", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, HistoryMod, lang.MUXKIOSK.HISTORY.MAIN, "historymod", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, HistoryRem, lang.MUXKIOSK.HISTORY.REMOVE, "historyrem", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Info, lang.MUXKIOSK.INFO, "info", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Advanced, lang.MUXKIOSK.ADVANCED, "advanced", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, General, lang.MUXKIOSK.GENERAL, "general", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Hdmi, lang.MUXKIOSK.HDMI, "hdmi", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Power, lang.MUXKIOSK.POWER, "power", allowed_restricted, 2);
    INIT_OPTION_ITEM(-1, kiosk, Visual, lang.MUXKIOSK.VISUAL, "visual", allowed_restricted, 2);

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

    decrease_option_value(lv_group_get_focused(ui_group_value), 1);
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value), 1);
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

    save_kiosk_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "launcher");

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
            {ui_lblNavB,       lang.GENERIC.SAVE,   0},
            {NULL, NULL,                            0}
    });

#define KIOSK(NAME, UDATA) lv_obj_set_user_data(ui_lbl##NAME##_kiosk, UDATA);
    KIOSK_ELEMENTS
#undef KIOSK

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

int muxkiosk_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXKIOSK.TITLE);
    init_muxkiosk(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_kiosk_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

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
