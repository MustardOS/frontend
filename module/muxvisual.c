#include "muxshare.h"
#include "muxvisual.h"
#include "../lvgl/lvgl.h"
#include "ui/ui_muxvisual.h"
#include <string.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/overlay.h"
#include "../common/input/list_nav.h"



static int overlay_count;

static int battery_original, mux_clock_original, network_original, name_original,
        dash_original, friendlyfolder_original, thetitleformat_original,
        titleincluderootdrive_original, folderitemcount_original,
        menu_counter_folder_original, overlayimage_original,
        overlaytransparency_original, display_empty_folder_original,
        menu_counter_file_original, hidden_original;

#define UI_COUNT 15
static lv_obj_t *ui_objects[UI_COUNT];

static lv_obj_t *ui_mux_panels[5];


struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBattery,               lang.MUXVISUAL.HELP.BATTERY},
            {ui_lblClock,                 lang.MUXVISUAL.HELP.CLOCK},
            {ui_lblNetwork,               lang.MUXVISUAL.HELP.NETWORK},
            {ui_lblName,                  lang.MUXVISUAL.HELP.NAME},
            {ui_lblDash,                  lang.MUXVISUAL.HELP.DASH},
            {ui_lblFriendlyFolder,        lang.MUXVISUAL.HELP.FRIENDLY},
            {ui_lblTheTitleFormat,        lang.MUXVISUAL.HELP.REFORMAT},
            {ui_lblTitleIncludeRootDrive, lang.MUXVISUAL.HELP.ROOT},
            {ui_lblFolderItemCount,       lang.MUXVISUAL.HELP.COUNT},
            {ui_lblDisplayEmptyFolder,    lang.MUXVISUAL.HELP.EMPTY},
            {ui_lblMenuCounterFolder,     lang.MUXVISUAL.HELP.COUNT_FOLDER},
            {ui_lblMenuCounterFile,       lang.MUXVISUAL.HELP.COUNT_FILE},
            {ui_lblHidden,                lang.MUXVISUAL.HELP.HIDDEN},
            {ui_lblOverlayImage,          lang.MUXVISUAL.HELP.OVERLAY_IMAGE},
            {ui_lblOverlayTransparency,   lang.MUXVISUAL.HELP.OVERLAY_TRANSPARENCY},
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

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

static void init_element_events() {
    lv_obj_t *dropdowns[] = {
            ui_droBattery_visual,
            ui_droClock,
            ui_droNetwork_visual,
            ui_droName,
            ui_droDash,
            ui_droFriendlyFolder,
            ui_droTheTitleFormat,
            ui_droTitleIncludeRootDrive,
            ui_droFolderItemCount,
            ui_droDisplayEmptyFolder,
            ui_droMenuCounterFolder,
            ui_droMenuCounterFile,
            ui_droHidden,
            ui_droOverlayImage,
            ui_droOverlayTransparency,
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }
}

static void init_dropdown_settings() {
    battery_original = lv_dropdown_get_selected(ui_droBattery_visual);
    mux_clock_original = lv_dropdown_get_selected(ui_droClock);
    network_original = lv_dropdown_get_selected(ui_droNetwork_visual);
    name_original = lv_dropdown_get_selected(ui_droName);
    dash_original = lv_dropdown_get_selected(ui_droDash);
    friendlyfolder_original = lv_dropdown_get_selected(ui_droFriendlyFolder);
    thetitleformat_original = lv_dropdown_get_selected(ui_droTheTitleFormat);
    titleincluderootdrive_original = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive);
    folderitemcount_original = lv_dropdown_get_selected(ui_droFolderItemCount);
    display_empty_folder_original = lv_dropdown_get_selected(ui_droDisplayEmptyFolder);
    menu_counter_folder_original = lv_dropdown_get_selected(ui_droMenuCounterFolder);
    menu_counter_file_original = lv_dropdown_get_selected(ui_droMenuCounterFile);
    hidden_original = lv_dropdown_get_selected(ui_droHidden);
    overlayimage_original = lv_dropdown_get_selected(ui_droOverlayImage);
    overlaytransparency_original = lv_dropdown_get_selected(ui_droOverlayTransparency);
}

static void restore_visual_options() {
    lv_dropdown_set_selected(ui_droBattery_visual, config.VISUAL.BATTERY);
    lv_dropdown_set_selected(ui_droClock, config.VISUAL.CLOCK);
    lv_dropdown_set_selected(ui_droNetwork_visual, config.VISUAL.NETWORK);
    lv_dropdown_set_selected(ui_droName, config.VISUAL.NAME);
    lv_dropdown_set_selected(ui_droDash, config.VISUAL.DASH);
    lv_dropdown_set_selected(ui_droFriendlyFolder, config.VISUAL.FRIENDLYFOLDER);
    lv_dropdown_set_selected(ui_droTheTitleFormat, config.VISUAL.THETITLEFORMAT);
    lv_dropdown_set_selected(ui_droTitleIncludeRootDrive, config.VISUAL.TITLEINCLUDEROOTDRIVE);
    lv_dropdown_set_selected(ui_droFolderItemCount, config.VISUAL.FOLDERITEMCOUNT);
    lv_dropdown_set_selected(ui_droDisplayEmptyFolder, config.VISUAL.FOLDEREMPTY);
    lv_dropdown_set_selected(ui_droMenuCounterFolder, config.VISUAL.COUNTERFOLDER);
    lv_dropdown_set_selected(ui_droMenuCounterFile, config.VISUAL.COUNTERFILE);
    lv_dropdown_set_selected(ui_droHidden, config.SETTINGS.GENERAL.HIDDEN);
    lv_dropdown_set_selected(ui_droOverlayImage, (config.VISUAL.OVERLAY_IMAGE > overlay_count) ? 0 : config.VISUAL.OVERLAY_IMAGE);
    lv_dropdown_set_selected(ui_droOverlayTransparency, config.VISUAL.OVERLAY_TRANSPARENCY);
}

static void save_visual_options() {
    int idx_battery = lv_dropdown_get_selected(ui_droBattery_visual);
    int idx_clock = lv_dropdown_get_selected(ui_droClock);
    int idx_network = lv_dropdown_get_selected(ui_droNetwork_visual);
    int idx_name = lv_dropdown_get_selected(ui_droName);
    int idx_dash = lv_dropdown_get_selected(ui_droDash);
    int idx_friendlyfolder = lv_dropdown_get_selected(ui_droFriendlyFolder);
    int idx_thetitleformat = lv_dropdown_get_selected(ui_droTheTitleFormat);
    int idx_titleincluderootdrive = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive);
    int idx_folderitemcount = lv_dropdown_get_selected(ui_droFolderItemCount);
    int idx_folderempty = lv_dropdown_get_selected(ui_droDisplayEmptyFolder);
    int idx_counterfolder = lv_dropdown_get_selected(ui_droMenuCounterFolder);
    int idx_counterfile = lv_dropdown_get_selected(ui_droMenuCounterFile);
    int idx_hidden = lv_dropdown_get_selected(ui_droHidden);
    int idx_overlayimage = lv_dropdown_get_selected(ui_droOverlayImage);
    int idx_overlaytransparency = lv_dropdown_get_selected(ui_droOverlayTransparency);

    if (lv_dropdown_get_selected(ui_droBattery_visual) != battery_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/battery"), "w", INT, idx_battery);
    }

    if (lv_dropdown_get_selected(ui_droClock) != mux_clock_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/clock"), "w", INT, idx_clock);
    }

    if (lv_dropdown_get_selected(ui_droNetwork_visual) != network_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/network"), "w", INT, idx_network);
    }

    if (lv_dropdown_get_selected(ui_droName) != name_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/name"), "w", INT, idx_name);
    }

    if (lv_dropdown_get_selected(ui_droDash) != dash_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/dash"), "w", INT, idx_dash);
    }

    if (lv_dropdown_get_selected(ui_droFriendlyFolder) != friendlyfolder_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/friendlyfolder"), "w", INT, idx_friendlyfolder);
    }

    if (lv_dropdown_get_selected(ui_droTheTitleFormat) != thetitleformat_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/thetitleformat"), "w", INT, idx_thetitleformat);
    }

    if (lv_dropdown_get_selected(ui_droTitleIncludeRootDrive) != titleincluderootdrive_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/titleincluderootdrive"), "w", INT, idx_titleincluderootdrive);
    }

    if (lv_dropdown_get_selected(ui_droFolderItemCount) != folderitemcount_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/folderitemcount"), "w", INT, idx_folderitemcount);
    }

    if (lv_dropdown_get_selected(ui_droDisplayEmptyFolder) != display_empty_folder_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/folderempty"), "w", INT, idx_folderempty);
    }

    if (lv_dropdown_get_selected(ui_droMenuCounterFolder) != menu_counter_folder_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/counterfolder"), "w", INT, idx_counterfolder);
    }

    if (lv_dropdown_get_selected(ui_droMenuCounterFile) != menu_counter_file_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/counterfile"), "w", INT, idx_counterfile);
    }

    if (lv_dropdown_get_selected(ui_droHidden) != hidden_original) {
        write_text_to_file((RUN_GLOBAL_PATH "settings/general/hidden"), "w", INT, idx_hidden);
    }

    if (lv_dropdown_get_selected(ui_droOverlayImage) != overlayimage_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/overlayimage"), "w", INT, idx_overlayimage);
    }

    if (lv_dropdown_get_selected(ui_droOverlayTransparency) != overlaytransparency_original) {
        write_text_to_file((RUN_GLOBAL_PATH "visual/overlaytransparency"), "w", INT, idx_overlaytransparency);
    }
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBattery,
            ui_pnlClock,
            ui_pnlNetwork,
            ui_pnlDash,
            ui_pnlName,
            ui_pnlDisplayEmptyFolder,
            ui_pnlTheTitleFormat,
            ui_pnlFolderItemCount,
            ui_pnlFriendlyFolder,
            ui_pnlMenuCounterFile,
            ui_pnlMenuCounterFolder,
            ui_pnlOverlayImage,
            ui_pnlOverlayTransparency,
            ui_pnlHidden,
            ui_pnlTitleIncludeRootDrive,
    };

    ui_objects[0] = ui_lblBattery;
    ui_objects[1] = ui_lblClock;
    ui_objects[2] = ui_lblNetwork;
    ui_objects[3] = ui_lblDash;
    ui_objects[4] = ui_lblName;
    ui_objects[5] = ui_lblDisplayEmptyFolder;
    ui_objects[6] = ui_lblTheTitleFormat;
    ui_objects[7] = ui_lblFolderItemCount;
    ui_objects[8] = ui_lblFriendlyFolder;
    ui_objects[9] = ui_lblMenuCounterFile;
    ui_objects[10] = ui_lblMenuCounterFolder;
    ui_objects[11] = ui_lblOverlayImage;
    ui_objects[12] = ui_lblOverlayTransparency;
    ui_objects[13] = ui_lblHidden;
    ui_objects[14] = ui_lblTitleIncludeRootDrive;
    lv_obj_t *ui_objects_value[] = {
            ui_droBattery_visual,
            ui_droClock,
            ui_droNetwork_visual,
            ui_droDash,
            ui_droName,
            ui_droDisplayEmptyFolder,
            ui_droTheTitleFormat,
            ui_droFolderItemCount,
            ui_droFriendlyFolder,
            ui_droMenuCounterFile,
            ui_droMenuCounterFolder,
            ui_droOverlayImage,
            ui_droOverlayTransparency,
            ui_droHidden,
            ui_droTitleIncludeRootDrive
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoBattery,
            ui_icoClock,
            ui_icoNetwork,
            ui_icoDash,
            ui_icoName,
            ui_icoDisplayEmptyFolder,
            ui_icoTheTitleFormat,
            ui_icoFolderItemCount,
            ui_icoFriendlyFolder,
            ui_icoMenuCounterFile,
            ui_icoMenuCounterFolder,
            ui_icoOverlayImage,
            ui_icoOverlayTransparency,
            ui_icoHidden,
            ui_icoTitleIncludeRootDrive
    };

    apply_theme_list_panel(ui_pnlBattery);
    apply_theme_list_panel(ui_pnlClock);
    apply_theme_list_panel(ui_pnlNetwork);
    apply_theme_list_panel(ui_pnlName);
    apply_theme_list_panel(ui_pnlDash);
    apply_theme_list_panel(ui_pnlFriendlyFolder);
    apply_theme_list_panel(ui_pnlTheTitleFormat);
    apply_theme_list_panel(ui_pnlTitleIncludeRootDrive);
    apply_theme_list_panel(ui_pnlFolderItemCount);
    apply_theme_list_panel(ui_pnlDisplayEmptyFolder);
    apply_theme_list_panel(ui_pnlMenuCounterFolder);
    apply_theme_list_panel(ui_pnlOverlayImage);
    apply_theme_list_panel(ui_pnlOverlayTransparency);
    apply_theme_list_panel(ui_pnlHidden);
    apply_theme_list_panel(ui_pnlMenuCounterFile);

    apply_theme_list_item(&theme, ui_lblBattery, lang.MUXVISUAL.BATTERY);
    apply_theme_list_item(&theme, ui_lblClock, lang.MUXVISUAL.CLOCK);
    apply_theme_list_item(&theme, ui_lblNetwork, lang.MUXVISUAL.NETWORK);
    apply_theme_list_item(&theme, ui_lblName, lang.MUXVISUAL.NAME.TITLE);
    apply_theme_list_item(&theme, ui_lblDash, lang.MUXVISUAL.DASH);
    apply_theme_list_item(&theme, ui_lblFriendlyFolder, lang.MUXVISUAL.FRIENDLY);
    apply_theme_list_item(&theme, ui_lblTheTitleFormat, lang.MUXVISUAL.REFORMAT);
    apply_theme_list_item(&theme, ui_lblTitleIncludeRootDrive, lang.MUXVISUAL.ROOT);
    apply_theme_list_item(&theme, ui_lblFolderItemCount, lang.MUXVISUAL.COUNT);
    apply_theme_list_item(&theme, ui_lblDisplayEmptyFolder, lang.MUXVISUAL.EMPTY);
    apply_theme_list_item(&theme, ui_lblMenuCounterFolder, lang.MUXVISUAL.COUNT_FOLDER);
    apply_theme_list_item(&theme, ui_lblOverlayImage, lang.MUXVISUAL.OVERLAY.IMAGE);
    apply_theme_list_item(&theme, ui_lblOverlayTransparency, lang.MUXVISUAL.OVERLAY.TRANSPARENCY);
    apply_theme_list_item(&theme, ui_lblHidden, lang.MUXVISUAL.HIDDEN);
    apply_theme_list_item(&theme, ui_lblMenuCounterFile, lang.MUXVISUAL.COUNT_FILE);

    apply_theme_list_glyph(&theme, ui_icoBattery, mux_module, "battery");
    apply_theme_list_glyph(&theme, ui_icoClock, mux_module, "clock");
    apply_theme_list_glyph(&theme, ui_icoNetwork, mux_module, "network");
    apply_theme_list_glyph(&theme, ui_icoName, mux_module, "name");
    apply_theme_list_glyph(&theme, ui_icoDash, mux_module, "dash");
    apply_theme_list_glyph(&theme, ui_icoFriendlyFolder, mux_module, "friendlyfolder");
    apply_theme_list_glyph(&theme, ui_icoTheTitleFormat, mux_module, "thetitleformat");
    apply_theme_list_glyph(&theme, ui_icoTitleIncludeRootDrive, mux_module, "titleincluderootdrive");
    apply_theme_list_glyph(&theme, ui_icoFolderItemCount, mux_module, "folderitemcount");
    apply_theme_list_glyph(&theme, ui_icoDisplayEmptyFolder, mux_module, "folderempty");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFolder, mux_module, "counterfolder");
    apply_theme_list_glyph(&theme, ui_icoOverlayImage, mux_module, "overlayimage");
    apply_theme_list_glyph(&theme, ui_icoOverlayTransparency, mux_module, "overlaytransparency");
    apply_theme_list_glyph(&theme, ui_icoHidden, mux_module, "hidden");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFile, mux_module, "counterfile");

    apply_theme_list_drop_down(&theme, ui_droBattery_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droClock, NULL);
    apply_theme_list_drop_down(&theme, ui_droNetwork_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droName, NULL);
    apply_theme_list_drop_down(&theme, ui_droDash, NULL);
    apply_theme_list_drop_down(&theme, ui_droFriendlyFolder, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheTitleFormat, NULL);
    apply_theme_list_drop_down(&theme, ui_droTitleIncludeRootDrive, NULL);
    apply_theme_list_drop_down(&theme, ui_droFolderItemCount, NULL);
    apply_theme_list_drop_down(&theme, ui_droDisplayEmptyFolder, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFolder, NULL);
    apply_theme_list_drop_down(&theme, ui_droOverlayImage, NULL);
    apply_theme_list_drop_down(&theme, ui_droHidden, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFile, NULL);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droBattery_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droClock, disabled_enabled, 2);
    add_drop_down_options(ui_droNetwork_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droHidden, disabled_enabled, 2);

    add_drop_down_options(ui_droName, (char *[]) {
            lang.MUXVISUAL.NAME.FULL,
            lang.MUXVISUAL.NAME.REM_SQ,
            lang.MUXVISUAL.NAME.REM_PA,
            lang.MUXVISUAL.NAME.REM_SQPA}, 4);

    add_drop_down_options(ui_droDash, disabled_enabled, 2);
    add_drop_down_options(ui_droFriendlyFolder, disabled_enabled, 2);
    add_drop_down_options(ui_droTheTitleFormat, disabled_enabled, 2);
    add_drop_down_options(ui_droTitleIncludeRootDrive, disabled_enabled, 2);
    add_drop_down_options(ui_droFolderItemCount, disabled_enabled, 2);
    add_drop_down_options(ui_droDisplayEmptyFolder, disabled_enabled, 2);
    add_drop_down_options(ui_droMenuCounterFolder, disabled_enabled, 2);
    add_drop_down_options(ui_droMenuCounterFile, disabled_enabled, 2);

    overlay_count = load_overlay_set(ui_droOverlayImage);

    char *transparency_string = generate_number_string(0, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOverlayTransparency, transparency_string);
    free(transparency_string);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = sizeof(ui_objects) / sizeof(ui_objects[0]);
    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlNetwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlNetwork, LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
    }
}

static void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (!current_item_index) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_value, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    play_sound("navigate", nav_sound, 0, 0);
    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound("back", nav_sound, 0, 1);

    save_visual_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");
    safe_quit(0);
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
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

    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblBattery, "battery");
    lv_obj_set_user_data(ui_lblClock, "clock");
    lv_obj_set_user_data(ui_lblNetwork, "network");
    lv_obj_set_user_data(ui_lblName, "name");
    lv_obj_set_user_data(ui_lblDash, "dash");
    lv_obj_set_user_data(ui_lblFriendlyFolder, "friendlyfolder");
    lv_obj_set_user_data(ui_lblTheTitleFormat, "thetitleformat");
    lv_obj_set_user_data(ui_lblTitleIncludeRootDrive, "titleincluderootdrive");
    lv_obj_set_user_data(ui_lblFolderItemCount, "folderitemcount");
    lv_obj_set_user_data(ui_lblDisplayEmptyFolder, "folderempty");
    lv_obj_set_user_data(ui_lblMenuCounterFolder, "counterfolder");
    lv_obj_set_user_data(ui_lblMenuCounterFile, "counterfile");
    lv_obj_set_user_data(ui_lblHidden, "hidden");
    lv_obj_set_user_data(ui_lblOverlayImage, "overlayimage");
    lv_obj_set_user_data(ui_lblOverlayTransparency, "overlaytransparency");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxvisual_main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    
            
    init_theme(1, 0);
    
    init_ui_common_screen(&theme, &device, &lang, lang.MUXVISUAL.TITLE);
    init_muxvisual(ui_pnlContent);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_element_events();
    init_navigation_sound(&nav_sound, mux_module);

    restore_visual_options();
    init_dropdown_settings();

    load_kiosk(&kiosk);

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
