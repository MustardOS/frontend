#include "muxshare.h"
#include "muxkiosk.h"
#include "ui/ui_muxkiosk.h"
#include <string.h>
#include <stdio.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int Enable_original, Archive_original, Task_original, Custom_original, Language_original, Network_original,
        Storage_original, WebServ_original, Core_original, Governor_original, Option_original, RetroArch_original,
        Search_original, Tag_original, Bootlogo_original, Catalogue_original, RAConfig_original, Theme_original,
        Clock_original, Timezone_original, Apps_original, Config_original, Explore_original, Collection_original,
        History_original, Info_original, Advanced_original, General_original, HDMI_original, Power_original,
        Visual_original;

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblEnable_kiosk,     lang.MUXKIOSK.HELP.ENABLE},
            {ui_lblArchive_kiosk,    lang.MUXKIOSK.HELP.ARCHIVE},
            {ui_lblTask_kiosk,       lang.MUXKIOSK.HELP.TASK},
            {ui_lblCustom_kiosk,     lang.MUXKIOSK.HELP.CUSTOM},
            {ui_lblLanguage_kiosk,   lang.MUXKIOSK.HELP.LANGUAGE},
            {ui_lblNetwork_kiosk,    lang.MUXKIOSK.HELP.NETWORK},
            {ui_lblStorage_kiosk,    lang.MUXKIOSK.HELP.STORAGE},
            {ui_lblWebServ_kiosk,    lang.MUXKIOSK.HELP.WEBSERV},
            {ui_lblCore_kiosk,       lang.MUXKIOSK.HELP.CORE},
            {ui_lblGovernor_kiosk,   lang.MUXKIOSK.HELP.GOVERNOR},
            {ui_lblOption_kiosk,     lang.MUXKIOSK.HELP.OPTION},
            {ui_lblRetroArch_kiosk,  lang.MUXKIOSK.HELP.RETROARCH},
            {ui_lblSearch_kiosk,     lang.MUXKIOSK.HELP.SEARCH},
            {ui_lblTag_kiosk,        lang.MUXKIOSK.HELP.TAG},
            {ui_lblBootlogo_kiosk,   lang.MUXKIOSK.HELP.BOOTLOGO},
            {ui_lblCatalogue_kiosk,  lang.MUXKIOSK.HELP.CATALOGUE},
            {ui_lblRAConfig_kiosk,   lang.MUXKIOSK.HELP.RACONFIG},
            {ui_lblTheme_kiosk,      lang.MUXKIOSK.HELP.THEME},
            {ui_lblClock_kiosk,      lang.MUXKIOSK.HELP.CLOCK},
            {ui_lblTimezone_kiosk,   lang.MUXKIOSK.HELP.TIMEZONE},
            {ui_lblApps_kiosk,       lang.MUXKIOSK.HELP.APPS},
            {ui_lblConfig_kiosk,     lang.MUXKIOSK.HELP.CONFIG},
            {ui_lblExplore_kiosk,    lang.MUXKIOSK.HELP.EXPLORE},
            {ui_lblCollection_kiosk, lang.MUXKIOSK.HELP.COLLECTION},
            {ui_lblHistory_kiosk,    lang.MUXKIOSK.HELP.HISTORY},
            {ui_lblInfo_kiosk,       lang.MUXKIOSK.HELP.INFO},
            {ui_lblAdvanced_kiosk,   lang.MUXKIOSK.HELP.ADVANCED},
            {ui_lblGeneral_kiosk,    lang.MUXKIOSK.HELP.GENERAL},
            {ui_lblHDMI_kiosk,       lang.MUXKIOSK.HELP.HDMI},
            {ui_lblPower_kiosk,      lang.MUXKIOSK.HELP.POWER},
            {ui_lblVisual_kiosk,     lang.MUXKIOSK.HELP.VISUAL},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void init_dropdown_settings() {
    Enable_original = lv_dropdown_get_selected(ui_droEnable_kiosk);
    Archive_original = lv_dropdown_get_selected(ui_droArchive_kiosk);
    Task_original = lv_dropdown_get_selected(ui_droTask_kiosk);
    Custom_original = lv_dropdown_get_selected(ui_droCustom_kiosk);
    Language_original = lv_dropdown_get_selected(ui_droLanguage_kiosk);
    Network_original = lv_dropdown_get_selected(ui_droNetwork_kiosk);
    Storage_original = lv_dropdown_get_selected(ui_droStorage_kiosk);
    WebServ_original = lv_dropdown_get_selected(ui_droWebServ_kiosk);
    Core_original = lv_dropdown_get_selected(ui_droCore_kiosk);
    Governor_original = lv_dropdown_get_selected(ui_droGovernor_kiosk);
    Option_original = lv_dropdown_get_selected(ui_droOption_kiosk);
    RetroArch_original = lv_dropdown_get_selected(ui_droRetroArch_kiosk);
    Search_original = lv_dropdown_get_selected(ui_droSearch_kiosk);
    Tag_original = lv_dropdown_get_selected(ui_droTag_kiosk);
    Bootlogo_original = lv_dropdown_get_selected(ui_droBootlogo_kiosk);
    Catalogue_original = lv_dropdown_get_selected(ui_droCatalogue_kiosk);
    RAConfig_original = lv_dropdown_get_selected(ui_droRAConfig_kiosk);
    Theme_original = lv_dropdown_get_selected(ui_droTheme_kiosk);
    Clock_original = lv_dropdown_get_selected(ui_droClock_kiosk);
    Timezone_original = lv_dropdown_get_selected(ui_droTimezone_kiosk);
    Apps_original = lv_dropdown_get_selected(ui_droApps_kiosk);
    Config_original = lv_dropdown_get_selected(ui_droConfig_kiosk);
    Explore_original = lv_dropdown_get_selected(ui_droExplore_kiosk);
    Collection_original = lv_dropdown_get_selected(ui_droCollection_kiosk);
    History_original = lv_dropdown_get_selected(ui_droHistory_kiosk);
    Info_original = lv_dropdown_get_selected(ui_droInfo_kiosk);
    Advanced_original = lv_dropdown_get_selected(ui_droAdvanced_kiosk);
    General_original = lv_dropdown_get_selected(ui_droGeneral_kiosk);
    HDMI_original = lv_dropdown_get_selected(ui_droHDMI_kiosk);
    Power_original = lv_dropdown_get_selected(ui_droPower_kiosk);
    Visual_original = lv_dropdown_get_selected(ui_droVisual_kiosk);
}

static void restore_kiosk_options() {
    lv_dropdown_set_selected(ui_droEnable_kiosk, kiosk.ENABLE);
    lv_dropdown_set_selected(ui_droArchive_kiosk, kiosk.APPLICATION.ARCHIVE);
    lv_dropdown_set_selected(ui_droTask_kiosk, kiosk.APPLICATION.TASK);
    lv_dropdown_set_selected(ui_droCustom_kiosk, kiosk.CONFIG.CUSTOMISATION);
    lv_dropdown_set_selected(ui_droLanguage_kiosk, kiosk.CONFIG.LANGUAGE);
    lv_dropdown_set_selected(ui_droNetwork_kiosk, kiosk.CONFIG.NETWORK);
    lv_dropdown_set_selected(ui_droStorage_kiosk, kiosk.CONFIG.STORAGE);
    lv_dropdown_set_selected(ui_droWebServ_kiosk, kiosk.CONFIG.WEB_SERVICES);
    lv_dropdown_set_selected(ui_droCore_kiosk, kiosk.CONTENT.CORE);
    lv_dropdown_set_selected(ui_droGovernor_kiosk, kiosk.CONTENT.GOVERNOR);
    lv_dropdown_set_selected(ui_droOption_kiosk, kiosk.CONTENT.OPTION);
    lv_dropdown_set_selected(ui_droRetroArch_kiosk, kiosk.CONTENT.RETROARCH);
    lv_dropdown_set_selected(ui_droSearch_kiosk, kiosk.CONTENT.SEARCH);
    lv_dropdown_set_selected(ui_droTag_kiosk, kiosk.CONTENT.TAG);
    lv_dropdown_set_selected(ui_droBootlogo_kiosk, kiosk.CUSTOM.BOOTLOGO);
    lv_dropdown_set_selected(ui_droCatalogue_kiosk, kiosk.CUSTOM.CATALOGUE);
    lv_dropdown_set_selected(ui_droRAConfig_kiosk, kiosk.CUSTOM.CONFIGURATION);
    lv_dropdown_set_selected(ui_droTheme_kiosk, kiosk.CUSTOM.THEME);
    lv_dropdown_set_selected(ui_droClock_kiosk, kiosk.DATETIME.CLOCK);
    lv_dropdown_set_selected(ui_droTimezone_kiosk, kiosk.DATETIME.TIMEZONE);
    lv_dropdown_set_selected(ui_droApps_kiosk, kiosk.LAUNCH.APPLICATION);
    lv_dropdown_set_selected(ui_droConfig_kiosk, kiosk.LAUNCH.CONFIGURATION);
    lv_dropdown_set_selected(ui_droExplore_kiosk, kiosk.LAUNCH.EXPLORE);
    lv_dropdown_set_selected(ui_droCollection_kiosk, kiosk.LAUNCH.COLLECTION);
    lv_dropdown_set_selected(ui_droHistory_kiosk, kiosk.LAUNCH.HISTORY);
    lv_dropdown_set_selected(ui_droInfo_kiosk, kiosk.LAUNCH.INFORMATION);
    lv_dropdown_set_selected(ui_droAdvanced_kiosk, kiosk.SETTING.ADVANCED);
    lv_dropdown_set_selected(ui_droGeneral_kiosk, kiosk.SETTING.GENERAL);
    lv_dropdown_set_selected(ui_droHDMI_kiosk, kiosk.SETTING.HDMI);
    lv_dropdown_set_selected(ui_droPower_kiosk, kiosk.SETTING.POWER);
    lv_dropdown_set_selected(ui_droVisual_kiosk, kiosk.SETTING.VISUAL);
}

static void save_kiosk_options() {
    int is_modified = 0;

    CHECK_AND_SAVE_KSK(kiosk, Enable, "enable", INT);
    CHECK_AND_SAVE_KSK(kiosk, Archive, "application/archive", INT);
    CHECK_AND_SAVE_KSK(kiosk, Task, "application/task", INT);
    CHECK_AND_SAVE_KSK(kiosk, Custom, "config/customisation", INT);
    CHECK_AND_SAVE_KSK(kiosk, Language, "config/language", INT);
    CHECK_AND_SAVE_KSK(kiosk, Network, "config/network", INT);
    CHECK_AND_SAVE_KSK(kiosk, Storage, "config/storage", INT);
    CHECK_AND_SAVE_KSK(kiosk, WebServ, "config/webserv", INT);
    CHECK_AND_SAVE_KSK(kiosk, Core, "content/core", INT);
    CHECK_AND_SAVE_KSK(kiosk, Governor, "content/governor", INT);
    CHECK_AND_SAVE_KSK(kiosk, Option, "content/option", INT);
    CHECK_AND_SAVE_KSK(kiosk, RetroArch, "content/retroarch", INT);
    CHECK_AND_SAVE_KSK(kiosk, Search, "content/search", INT);
    CHECK_AND_SAVE_KSK(kiosk, Tag, "content/tag", INT);
    CHECK_AND_SAVE_KSK(kiosk, Bootlogo, "custom/bootlogo", INT);
    CHECK_AND_SAVE_KSK(kiosk, Catalogue, "custom/catalogue", INT);
    CHECK_AND_SAVE_KSK(kiosk, RAConfig, "custom/raconfig", INT);
    CHECK_AND_SAVE_KSK(kiosk, Theme, "custom/theme", INT);
    CHECK_AND_SAVE_KSK(kiosk, Clock, "datetime/clock", INT);
    CHECK_AND_SAVE_KSK(kiosk, Timezone, "datetime/timezone", INT);
    CHECK_AND_SAVE_KSK(kiosk, Apps, "launch/apps", INT);
    CHECK_AND_SAVE_KSK(kiosk, Config, "launch/config", INT);
    CHECK_AND_SAVE_KSK(kiosk, Explore, "launch/explore", INT);
    CHECK_AND_SAVE_KSK(kiosk, Collection, "launch/collection", INT);
    CHECK_AND_SAVE_KSK(kiosk, History, "launch/history", INT);
    CHECK_AND_SAVE_KSK(kiosk, Info, "launch/info", INT);
    CHECK_AND_SAVE_KSK(kiosk, Advanced, "setting/advanced", INT);
    CHECK_AND_SAVE_KSK(kiosk, General, "setting/general", INT);
    CHECK_AND_SAVE_KSK(kiosk, HDMI, "setting/hdmi", INT);
    CHECK_AND_SAVE_KSK(kiosk, Power, "setting/power", INT);
    CHECK_AND_SAVE_KSK(kiosk, Visual, "setting/visual", INT);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);
        refresh_kiosk = 1;
    }
}

static void init_navigation_group() {
#define UI_COUNT 31

    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    int ui_index = 0;

    INIT_OPTION_ITEM(kiosk, Enable, lang.MUXKIOSK.ENABLE, "enable", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Archive, lang.MUXKIOSK.ARCHIVE, "archive", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Task, lang.MUXKIOSK.TASK, "task", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Custom, lang.MUXKIOSK.CUSTOM, "custom", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Language, lang.MUXKIOSK.LANGUAGE, "language", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Network, lang.MUXKIOSK.NETWORK, "network", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Storage, lang.MUXKIOSK.STORAGE, "storage", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, WebServ, lang.MUXKIOSK.WEBSERV, "webserv", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Core, lang.MUXKIOSK.CORE, "core", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Governor, lang.MUXKIOSK.GOVERNOR, "governor", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Option, lang.MUXKIOSK.OPTION, "option", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, RetroArch, lang.MUXKIOSK.RETROARCH, "retroarch", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Search, lang.MUXKIOSK.SEARCH, "search", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Tag, lang.MUXKIOSK.TAG, "tag", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Bootlogo, lang.MUXKIOSK.BOOTLOGO, "bootlogo", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Catalogue, lang.MUXKIOSK.CATALOGUE, "catalogue", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, RAConfig, lang.MUXKIOSK.RACONFIG, "raconfig", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Theme, lang.MUXKIOSK.THEME, "theme", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Clock, lang.MUXKIOSK.CLOCK, "clock", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Timezone, lang.MUXKIOSK.TIMEZONE, "timezone", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Apps, lang.MUXKIOSK.APPS, "apps", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Config, lang.MUXKIOSK.CONFIG, "config", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Explore, lang.MUXKIOSK.EXPLORE, "explore", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Collection, lang.MUXKIOSK.COLLECTION, "collection", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, History, lang.MUXKIOSK.HISTORY, "history", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Info, lang.MUXKIOSK.INFO, "info", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Advanced, lang.MUXKIOSK.ADVANCED, "advanced", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, General, lang.MUXKIOSK.GENERAL, "general", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, HDMI, lang.MUXKIOSK.HDMI, "hdmi", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Power, lang.MUXKIOSK.POWER, "power", disabled_enabled, 2);
    INIT_OPTION_ITEM(kiosk, Visual, lang.MUXKIOSK.VISUAL, "visual", disabled_enabled, 2);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = ui_index;

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

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
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
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblEnable_kiosk, "enable");
    lv_obj_set_user_data(ui_lblArchive_kiosk, "archive");
    lv_obj_set_user_data(ui_lblTask_kiosk, "task");
    lv_obj_set_user_data(ui_lblCustom_kiosk, "custom");
    lv_obj_set_user_data(ui_lblLanguage_kiosk, "language");
    lv_obj_set_user_data(ui_lblNetwork_kiosk, "network");
    lv_obj_set_user_data(ui_lblStorage_kiosk, "storage");
    lv_obj_set_user_data(ui_lblWebServ_kiosk, "webserv");
    lv_obj_set_user_data(ui_lblCore_kiosk, "core");
    lv_obj_set_user_data(ui_lblGovernor_kiosk, "governor");
    lv_obj_set_user_data(ui_lblOption_kiosk, "option");
    lv_obj_set_user_data(ui_lblRetroArch_kiosk, "retroarch");
    lv_obj_set_user_data(ui_lblSearch_kiosk, "search");
    lv_obj_set_user_data(ui_lblTag_kiosk, "tag");
    lv_obj_set_user_data(ui_lblBootlogo_kiosk, "bootlogo");
    lv_obj_set_user_data(ui_lblCatalogue_kiosk, "catalogue");
    lv_obj_set_user_data(ui_lblRAConfig_kiosk, "raconfig");
    lv_obj_set_user_data(ui_lblTheme_kiosk, "theme");
    lv_obj_set_user_data(ui_lblClock_kiosk, "clock");
    lv_obj_set_user_data(ui_lblTimezone_kiosk, "timezone");
    lv_obj_set_user_data(ui_lblApps_kiosk, "apps");
    lv_obj_set_user_data(ui_lblConfig_kiosk, "config");
    lv_obj_set_user_data(ui_lblExplore_kiosk, "explore");
    lv_obj_set_user_data(ui_lblCollection_kiosk, "collection");
    lv_obj_set_user_data(ui_lblHistory_kiosk, "history");
    lv_obj_set_user_data(ui_lblInfo_kiosk, "info");
    lv_obj_set_user_data(ui_lblAdvanced_kiosk, "advanced");
    lv_obj_set_user_data(ui_lblGeneral_kiosk, "general");
    lv_obj_set_user_data(ui_lblHDMI_kiosk, "hdmi");
    lv_obj_set_user_data(ui_lblPower_kiosk, "power");
    lv_obj_set_user_data(ui_lblVisual_kiosk, "visual");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    if (kiosk.ENABLE) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxkiosk_main() {
    init_module("muxkiosk");

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
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
