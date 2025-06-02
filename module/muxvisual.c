#include "muxshare.h"
#include "muxvisual.h"
#include "ui/ui_muxvisual.h"
#include <stdlib.h>
#include <string.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/overlay.h"
#include "../common/input/list_nav.h"

static int overlay_count;

static int Battery_original, Clock_original, Network_original, Name_original, Dash_original, FriendlyFolder_original,
        TheTitleFormat_original, TitleIncludeRootDrive_original, FolderItemCount_original, DisplayEmptyFolder_original,
        MenuCounterFolder_original, MenuCounterFile_original, Hidden_original, OverlayImage_original,
        OverlayTransparency_original;

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBattery_visual,               lang.MUXVISUAL.HELP.BATTERY},
            {ui_lblClock_visual,                 lang.MUXVISUAL.HELP.CLOCK},
            {ui_lblNetwork_visual,               lang.MUXVISUAL.HELP.NETWORK},
            {ui_lblName_visual,                  lang.MUXVISUAL.HELP.NAME},
            {ui_lblDash_visual,                  lang.MUXVISUAL.HELP.DASH},
            {ui_lblFriendlyFolder_visual,        lang.MUXVISUAL.HELP.FRIENDLY},
            {ui_lblTheTitleFormat_visual,        lang.MUXVISUAL.HELP.REFORMAT},
            {ui_lblTitleIncludeRootDrive_visual, lang.MUXVISUAL.HELP.ROOT},
            {ui_lblFolderItemCount_visual,       lang.MUXVISUAL.HELP.COUNT},
            {ui_lblDisplayEmptyFolder_visual,    lang.MUXVISUAL.HELP.EMPTY},
            {ui_lblMenuCounterFolder_visual,     lang.MUXVISUAL.HELP.COUNT_FOLDER},
            {ui_lblMenuCounterFile_visual,       lang.MUXVISUAL.HELP.COUNT_FILE},
            {ui_lblHidden_visual,                lang.MUXVISUAL.HELP.HIDDEN},
            {ui_lblOverlayImage_visual,          lang.MUXVISUAL.HELP.OVERLAY_IMAGE},
            {ui_lblOverlayTransparency_visual,   lang.MUXVISUAL.HELP.OVERLAY_TRANSPARENCY},
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
    Battery_original = lv_dropdown_get_selected(ui_droBattery_visual);
    Clock_original = lv_dropdown_get_selected(ui_droClock_visual);
    Network_original = lv_dropdown_get_selected(ui_droNetwork_visual);
    Name_original = lv_dropdown_get_selected(ui_droName_visual);
    Dash_original = lv_dropdown_get_selected(ui_droDash_visual);
    FriendlyFolder_original = lv_dropdown_get_selected(ui_droFriendlyFolder_visual);
    TheTitleFormat_original = lv_dropdown_get_selected(ui_droTheTitleFormat_visual);
    TitleIncludeRootDrive_original = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive_visual);
    FolderItemCount_original = lv_dropdown_get_selected(ui_droFolderItemCount_visual);
    DisplayEmptyFolder_original = lv_dropdown_get_selected(ui_droDisplayEmptyFolder_visual);
    MenuCounterFolder_original = lv_dropdown_get_selected(ui_droMenuCounterFolder_visual);
    MenuCounterFile_original = lv_dropdown_get_selected(ui_droMenuCounterFile_visual);
    Hidden_original = lv_dropdown_get_selected(ui_droHidden_visual);
    OverlayImage_original = lv_dropdown_get_selected(ui_droOverlayImage_visual);
    OverlayTransparency_original = lv_dropdown_get_selected(ui_droOverlayTransparency_visual);
}

static void restore_visual_options() {
    lv_dropdown_set_selected(ui_droBattery_visual, config.VISUAL.BATTERY);
    lv_dropdown_set_selected(ui_droClock_visual, config.VISUAL.CLOCK);
    lv_dropdown_set_selected(ui_droNetwork_visual, config.VISUAL.NETWORK);
    lv_dropdown_set_selected(ui_droName_visual, config.VISUAL.NAME);
    lv_dropdown_set_selected(ui_droDash_visual, config.VISUAL.DASH);
    lv_dropdown_set_selected(ui_droFriendlyFolder_visual, config.VISUAL.FRIENDLYFOLDER);
    lv_dropdown_set_selected(ui_droTheTitleFormat_visual, config.VISUAL.THETITLEFORMAT);
    lv_dropdown_set_selected(ui_droTitleIncludeRootDrive_visual, config.VISUAL.TITLEINCLUDEROOTDRIVE);
    lv_dropdown_set_selected(ui_droFolderItemCount_visual, config.VISUAL.FOLDERITEMCOUNT);
    lv_dropdown_set_selected(ui_droDisplayEmptyFolder_visual, config.VISUAL.FOLDEREMPTY);
    lv_dropdown_set_selected(ui_droMenuCounterFolder_visual, config.VISUAL.COUNTERFOLDER);
    lv_dropdown_set_selected(ui_droMenuCounterFile_visual, config.VISUAL.COUNTERFILE);
    lv_dropdown_set_selected(ui_droHidden_visual, config.SETTINGS.GENERAL.HIDDEN);
    lv_dropdown_set_selected(ui_droOverlayImage_visual, (config.VISUAL.OVERLAY_IMAGE > overlay_count) ? 0 : config.VISUAL.OVERLAY_IMAGE);
    lv_dropdown_set_selected(ui_droOverlayTransparency_visual, config.VISUAL.OVERLAY_TRANSPARENCY);
}

static void save_visual_options() {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(visual, Battery, "visual/battery", INT);
    CHECK_AND_SAVE_STD(visual, Clock, "visual/clock", INT);
    CHECK_AND_SAVE_STD(visual, Network, "visual/network", INT);
    CHECK_AND_SAVE_STD(visual, Name, "visual/name", INT);
    CHECK_AND_SAVE_STD(visual, Dash, "visual/dash", INT);
    CHECK_AND_SAVE_STD(visual, FriendlyFolder, "visual/friendlyfolder", INT);
    CHECK_AND_SAVE_STD(visual, TheTitleFormat, "visual/thetitleformat", INT);
    CHECK_AND_SAVE_STD(visual, TitleIncludeRootDrive, "visual/titleincluderootdrive", INT);
    CHECK_AND_SAVE_STD(visual, FolderItemCount, "visual/folderitemcount", INT);
    CHECK_AND_SAVE_STD(visual, DisplayEmptyFolder, "visual/folderempty", INT);
    CHECK_AND_SAVE_STD(visual, MenuCounterFolder, "visual/counterfolder", INT);
    CHECK_AND_SAVE_STD(visual, MenuCounterFile, "visual/counterfile", INT);
    CHECK_AND_SAVE_STD(visual, Hidden, "settings/general/hidden", INT);
    CHECK_AND_SAVE_STD(visual, OverlayImage, "visual/overlayimage", INT);
    CHECK_AND_SAVE_STD(visual, OverlayTransparency, "visual/overlaytransparency", INT);

#undef CHECK_AND_SAVE

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);
        refresh_config = 1;
    }
}

static void init_navigation_group() {
#define UI_COUNT 15

    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    int ui_index = 0;

    char *visual_names[] = {
            lang.MUXVISUAL.NAME.FULL,
            lang.MUXVISUAL.NAME.REM_SQ,
            lang.MUXVISUAL.NAME.REM_PA,
            lang.MUXVISUAL.NAME.REM_SQPA
    };

    INIT_OPTION_ITEM(visual, Battery, lang.MUXVISUAL.BATTERY, "battery", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, Clock, lang.MUXVISUAL.CLOCK, "clock", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, Network, lang.MUXVISUAL.NETWORK, "network", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, Name, lang.MUXVISUAL.NAME.TITLE, "name", visual_names, 4);
    INIT_OPTION_ITEM(visual, Dash, lang.MUXVISUAL.DASH, "dash", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, FriendlyFolder, lang.MUXVISUAL.FRIENDLY, "friendlyfolder", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, TheTitleFormat, lang.MUXVISUAL.REFORMAT, "thetitleformat", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, TitleIncludeRootDrive, lang.MUXVISUAL.ROOT, "titleincluderootdrive", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, FolderItemCount, lang.MUXVISUAL.COUNT, "folderitemcount", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, DisplayEmptyFolder, lang.MUXVISUAL.EMPTY, "folderempty", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, MenuCounterFolder, lang.MUXVISUAL.COUNT_FOLDER, "counterfolder", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, MenuCounterFile, lang.MUXVISUAL.COUNT_FILE, "counterfile", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, Hidden, lang.MUXVISUAL.HIDDEN, "hidden", disabled_enabled, 2);
    INIT_OPTION_ITEM(visual, OverlayImage, lang.MUXVISUAL.OVERLAY.IMAGE, "overlayimage", NULL, 0);
    INIT_OPTION_ITEM(visual, OverlayTransparency, lang.MUXVISUAL.OVERLAY.TRANSPARENCY, "overlaytransparency", NULL, 0);

    overlay_count = load_overlay_set(ui_droOverlayImage_visual);

    char *transparency_string = generate_number_string(0, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOverlayTransparency_visual, transparency_string);
    free(transparency_string);

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

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlNetwork_visual, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
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

    save_visual_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");
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

    lv_obj_set_user_data(ui_lblBattery_visual, "battery");
    lv_obj_set_user_data(ui_lblClock_visual, "clock");
    lv_obj_set_user_data(ui_lblNetwork_visual, "network");
    lv_obj_set_user_data(ui_lblName_visual, "name");
    lv_obj_set_user_data(ui_lblDash_visual, "dash");
    lv_obj_set_user_data(ui_lblFriendlyFolder_visual, "friendlyfolder");
    lv_obj_set_user_data(ui_lblTheTitleFormat_visual, "thetitleformat");
    lv_obj_set_user_data(ui_lblTitleIncludeRootDrive_visual, "titleincluderootdrive");
    lv_obj_set_user_data(ui_lblFolderItemCount_visual, "folderitemcount");
    lv_obj_set_user_data(ui_lblDisplayEmptyFolder_visual, "folderempty");
    lv_obj_set_user_data(ui_lblMenuCounterFolder_visual, "counterfolder");
    lv_obj_set_user_data(ui_lblMenuCounterFile_visual, "counterfile");
    lv_obj_set_user_data(ui_lblHidden_visual, "hidden");
    lv_obj_set_user_data(ui_lblOverlayImage_visual, "overlayimage");
    lv_obj_set_user_data(ui_lblOverlayTransparency_visual, "overlaytransparency");

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

int muxvisual_main() {
    init_module("muxvisual");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXVISUAL.TITLE);
    init_muxvisual(ui_pnlContent);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();

    restore_visual_options();
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
