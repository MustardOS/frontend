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

static int battery_original, mux_clock_original, network_original, name_original,
        dash_original, friendlyfolder_original, thetitleformat_original,
        titleincluderootdrive_original, folderitemcount_original,
        menu_counter_folder_original, overlayimage_original,
        overlaytransparency_original, display_empty_folder_original,
        menu_counter_file_original, hidden_original;

#define UI_COUNT 15
static lv_obj_t *ui_objects[UI_COUNT];

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
    battery_original = lv_dropdown_get_selected(ui_droBattery_visual_visual);
    mux_clock_original = lv_dropdown_get_selected(ui_droClock_visual);
    network_original = lv_dropdown_get_selected(ui_droNetwork_visual_visual);
    name_original = lv_dropdown_get_selected(ui_droName_visual);
    dash_original = lv_dropdown_get_selected(ui_droDash_visual);
    friendlyfolder_original = lv_dropdown_get_selected(ui_droFriendlyFolder_visual);
    thetitleformat_original = lv_dropdown_get_selected(ui_droTheTitleFormat_visual);
    titleincluderootdrive_original = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive_visual);
    folderitemcount_original = lv_dropdown_get_selected(ui_droFolderItemCount_visual);
    display_empty_folder_original = lv_dropdown_get_selected(ui_droDisplayEmptyFolder_visual);
    menu_counter_folder_original = lv_dropdown_get_selected(ui_droMenuCounterFolder_visual);
    menu_counter_file_original = lv_dropdown_get_selected(ui_droMenuCounterFile_visual);
    hidden_original = lv_dropdown_get_selected(ui_droHidden_visual);
    overlayimage_original = lv_dropdown_get_selected(ui_droOverlayImage_visual);
    overlaytransparency_original = lv_dropdown_get_selected(ui_droOverlayTransparency_visual);
}

static void restore_visual_options() {
    lv_dropdown_set_selected(ui_droBattery_visual_visual, config.VISUAL.BATTERY);
    lv_dropdown_set_selected(ui_droClock_visual, config.VISUAL.CLOCK);
    lv_dropdown_set_selected(ui_droNetwork_visual_visual, config.VISUAL.NETWORK);
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
    lv_dropdown_set_selected(ui_droOverlayImage_visual,
                             (config.VISUAL.OVERLAY_IMAGE > overlay_count) ? 0 : config.VISUAL.OVERLAY_IMAGE);
    lv_dropdown_set_selected(ui_droOverlayTransparency_visual, config.VISUAL.OVERLAY_TRANSPARENCY);
}

static void save_visual_options() {
    int idx_battery = lv_dropdown_get_selected(ui_droBattery_visual_visual);
    int idx_clock = lv_dropdown_get_selected(ui_droClock_visual);
    int idx_network = lv_dropdown_get_selected(ui_droNetwork_visual_visual);
    int idx_name = lv_dropdown_get_selected(ui_droName_visual);
    int idx_dash = lv_dropdown_get_selected(ui_droDash_visual);
    int idx_friendlyfolder = lv_dropdown_get_selected(ui_droFriendlyFolder_visual);
    int idx_thetitleformat = lv_dropdown_get_selected(ui_droTheTitleFormat_visual);
    int idx_titleincluderootdrive = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive_visual);
    int idx_folderitemcount = lv_dropdown_get_selected(ui_droFolderItemCount_visual);
    int idx_folderempty = lv_dropdown_get_selected(ui_droDisplayEmptyFolder_visual);
    int idx_counterfolder = lv_dropdown_get_selected(ui_droMenuCounterFolder_visual);
    int idx_counterfile = lv_dropdown_get_selected(ui_droMenuCounterFile_visual);
    int idx_hidden = lv_dropdown_get_selected(ui_droHidden_visual);
    int idx_overlayimage = lv_dropdown_get_selected(ui_droOverlayImage_visual);
    int idx_overlaytransparency = lv_dropdown_get_selected(ui_droOverlayTransparency_visual);

    int is_modified = 0;

    if (lv_dropdown_get_selected(ui_droBattery_visual_visual) != battery_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/battery"), "w", INT, idx_battery);
    }

    if (lv_dropdown_get_selected(ui_droClock_visual) != mux_clock_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/clock"), "w", INT, idx_clock);
    }

    if (lv_dropdown_get_selected(ui_droNetwork_visual_visual) != network_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/network"), "w", INT, idx_network);
    }

    if (lv_dropdown_get_selected(ui_droName_visual) != name_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/name"), "w", INT, idx_name);
    }

    if (lv_dropdown_get_selected(ui_droDash_visual) != dash_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/dash"), "w", INT, idx_dash);
    }

    if (lv_dropdown_get_selected(ui_droFriendlyFolder_visual) != friendlyfolder_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/friendlyfolder"), "w", INT, idx_friendlyfolder);
    }

    if (lv_dropdown_get_selected(ui_droTheTitleFormat_visual) != thetitleformat_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/thetitleformat"), "w", INT, idx_thetitleformat);
    }

    if (lv_dropdown_get_selected(ui_droTitleIncludeRootDrive_visual) != titleincluderootdrive_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/titleincluderootdrive"), "w", INT, idx_titleincluderootdrive);
    }

    if (lv_dropdown_get_selected(ui_droFolderItemCount_visual) != folderitemcount_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/folderitemcount"), "w", INT, idx_folderitemcount);
    }

    if (lv_dropdown_get_selected(ui_droDisplayEmptyFolder_visual) != display_empty_folder_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/folderempty"), "w", INT, idx_folderempty);
    }

    if (lv_dropdown_get_selected(ui_droMenuCounterFolder_visual) != menu_counter_folder_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/counterfolder"), "w", INT, idx_counterfolder);
    }

    if (lv_dropdown_get_selected(ui_droMenuCounterFile_visual) != menu_counter_file_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/counterfile"), "w", INT, idx_counterfile);
    }

    if (lv_dropdown_get_selected(ui_droHidden_visual) != hidden_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "settings/general/hidden"), "w", INT, idx_hidden);
    }

    if (lv_dropdown_get_selected(ui_droOverlayImage_visual) != overlayimage_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/overlayimage"), "w", INT, idx_overlayimage);
    }

    if (lv_dropdown_get_selected(ui_droOverlayTransparency_visual) != overlaytransparency_original) {
        is_modified++;
        write_text_to_file((CONF_CONFIG_PATH "visual/overlaytransparency"), "w", INT, idx_overlaytransparency);
    }

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);
    }

    refresh_config = 1;
}

static void init_navigation_group() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBattery_visual,
            ui_pnlClock_visual,
            ui_pnlNetwork_visual,
            ui_pnlDash_visual,
            ui_pnlName_visual,
            ui_pnlDisplayEmptyFolder_visual,
            ui_pnlTheTitleFormat_visual,
            ui_pnlFolderItemCount_visual,
            ui_pnlFriendlyFolder_visual,
            ui_pnlMenuCounterFile_visual,
            ui_pnlMenuCounterFolder_visual,
            ui_pnlOverlayImage_visual,
            ui_pnlOverlayTransparency_visual,
            ui_pnlHidden_visual,
            ui_pnlTitleIncludeRootDrive_visual,
    };

    ui_objects[0] = ui_lblBattery_visual;
    ui_objects[1] = ui_lblClock_visual;
    ui_objects[2] = ui_lblNetwork_visual;
    ui_objects[3] = ui_lblDash_visual;
    ui_objects[4] = ui_lblName_visual;
    ui_objects[5] = ui_lblDisplayEmptyFolder_visual;
    ui_objects[6] = ui_lblTheTitleFormat_visual;
    ui_objects[7] = ui_lblFolderItemCount_visual;
    ui_objects[8] = ui_lblFriendlyFolder_visual;
    ui_objects[9] = ui_lblMenuCounterFile_visual;
    ui_objects[10] = ui_lblMenuCounterFolder_visual;
    ui_objects[11] = ui_lblOverlayImage_visual;
    ui_objects[12] = ui_lblOverlayTransparency_visual;
    ui_objects[13] = ui_lblHidden_visual;
    ui_objects[14] = ui_lblTitleIncludeRootDrive_visual;

    lv_obj_t *ui_objects_value[] = {
            ui_droBattery_visual_visual,
            ui_droClock_visual,
            ui_droNetwork_visual_visual,
            ui_droDash_visual,
            ui_droName_visual,
            ui_droDisplayEmptyFolder_visual,
            ui_droTheTitleFormat_visual,
            ui_droFolderItemCount_visual,
            ui_droFriendlyFolder_visual,
            ui_droMenuCounterFile_visual,
            ui_droMenuCounterFolder_visual,
            ui_droOverlayImage_visual,
            ui_droOverlayTransparency_visual,
            ui_droHidden_visual,
            ui_droTitleIncludeRootDrive_visual
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoBattery_visual,
            ui_icoClock_visual,
            ui_icoNetwork_visual,
            ui_icoDash_visual,
            ui_icoName_visual,
            ui_icoDisplayEmptyFolder_visual,
            ui_icoTheTitleFormat_visual,
            ui_icoFolderItemCount_visual,
            ui_icoFriendlyFolder_visual,
            ui_icoMenuCounterFile_visual,
            ui_icoMenuCounterFolder_visual,
            ui_icoOverlayImage_visual,
            ui_icoOverlayTransparency_visual,
            ui_icoHidden_visual,
            ui_icoTitleIncludeRootDrive_visual
    };

    apply_theme_list_panel(ui_pnlBattery_visual);
    apply_theme_list_panel(ui_pnlClock_visual);
    apply_theme_list_panel(ui_pnlNetwork_visual);
    apply_theme_list_panel(ui_pnlName_visual);
    apply_theme_list_panel(ui_pnlDash_visual);
    apply_theme_list_panel(ui_pnlFriendlyFolder_visual);
    apply_theme_list_panel(ui_pnlTheTitleFormat_visual);
    apply_theme_list_panel(ui_pnlTitleIncludeRootDrive_visual);
    apply_theme_list_panel(ui_pnlFolderItemCount_visual);
    apply_theme_list_panel(ui_pnlDisplayEmptyFolder_visual);
    apply_theme_list_panel(ui_pnlMenuCounterFolder_visual);
    apply_theme_list_panel(ui_pnlOverlayImage_visual);
    apply_theme_list_panel(ui_pnlOverlayTransparency_visual);
    apply_theme_list_panel(ui_pnlHidden_visual);
    apply_theme_list_panel(ui_pnlMenuCounterFile_visual);

    apply_theme_list_item(&theme, ui_lblBattery_visual, lang.MUXVISUAL.BATTERY);
    apply_theme_list_item(&theme, ui_lblClock_visual, lang.MUXVISUAL.CLOCK);
    apply_theme_list_item(&theme, ui_lblNetwork_visual, lang.MUXVISUAL.NETWORK);
    apply_theme_list_item(&theme, ui_lblName_visual, lang.MUXVISUAL.NAME.TITLE);
    apply_theme_list_item(&theme, ui_lblDash_visual, lang.MUXVISUAL.DASH);
    apply_theme_list_item(&theme, ui_lblFriendlyFolder_visual, lang.MUXVISUAL.FRIENDLY);
    apply_theme_list_item(&theme, ui_lblTheTitleFormat_visual, lang.MUXVISUAL.REFORMAT);
    apply_theme_list_item(&theme, ui_lblTitleIncludeRootDrive_visual, lang.MUXVISUAL.ROOT);
    apply_theme_list_item(&theme, ui_lblFolderItemCount_visual, lang.MUXVISUAL.COUNT);
    apply_theme_list_item(&theme, ui_lblDisplayEmptyFolder_visual, lang.MUXVISUAL.EMPTY);
    apply_theme_list_item(&theme, ui_lblMenuCounterFolder_visual, lang.MUXVISUAL.COUNT_FOLDER);
    apply_theme_list_item(&theme, ui_lblOverlayImage_visual, lang.MUXVISUAL.OVERLAY.IMAGE);
    apply_theme_list_item(&theme, ui_lblOverlayTransparency_visual, lang.MUXVISUAL.OVERLAY.TRANSPARENCY);
    apply_theme_list_item(&theme, ui_lblHidden_visual, lang.MUXVISUAL.HIDDEN);
    apply_theme_list_item(&theme, ui_lblMenuCounterFile_visual, lang.MUXVISUAL.COUNT_FILE);

    apply_theme_list_glyph(&theme, ui_icoBattery_visual, mux_module, "battery");
    apply_theme_list_glyph(&theme, ui_icoClock_visual, mux_module, "clock");
    apply_theme_list_glyph(&theme, ui_icoNetwork_visual, mux_module, "network");
    apply_theme_list_glyph(&theme, ui_icoName_visual, mux_module, "name");
    apply_theme_list_glyph(&theme, ui_icoDash_visual, mux_module, "dash");
    apply_theme_list_glyph(&theme, ui_icoFriendlyFolder_visual, mux_module, "friendlyfolder");
    apply_theme_list_glyph(&theme, ui_icoTheTitleFormat_visual, mux_module, "thetitleformat");
    apply_theme_list_glyph(&theme, ui_icoTitleIncludeRootDrive_visual, mux_module, "titleincluderootdrive");
    apply_theme_list_glyph(&theme, ui_icoFolderItemCount_visual, mux_module, "folderitemcount");
    apply_theme_list_glyph(&theme, ui_icoDisplayEmptyFolder_visual, mux_module, "folderempty");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFolder_visual, mux_module, "counterfolder");
    apply_theme_list_glyph(&theme, ui_icoOverlayImage_visual, mux_module, "overlayimage");
    apply_theme_list_glyph(&theme, ui_icoOverlayTransparency_visual, mux_module, "overlaytransparency");
    apply_theme_list_glyph(&theme, ui_icoHidden_visual, mux_module, "hidden");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFile_visual, mux_module, "counterfile");

    apply_theme_list_drop_down(&theme, ui_droBattery_visual_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droClock_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droNetwork_visual_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droName_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droDash_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droFriendlyFolder_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheTitleFormat_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droTitleIncludeRootDrive_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droFolderItemCount_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droDisplayEmptyFolder_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFolder_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droOverlayImage_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droHidden_visual, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFile_visual, NULL);

    char *disabled_enabled[] = {lang.GENERIC.DISABLED, lang.GENERIC.ENABLED};
    add_drop_down_options(ui_droBattery_visual_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droClock_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droNetwork_visual_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droHidden_visual, disabled_enabled, 2);

    add_drop_down_options(ui_droName_visual, (char *[]) {
            lang.MUXVISUAL.NAME.FULL,
            lang.MUXVISUAL.NAME.REM_SQ,
            lang.MUXVISUAL.NAME.REM_PA,
            lang.MUXVISUAL.NAME.REM_SQPA}, 4);

    add_drop_down_options(ui_droDash_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droFriendlyFolder_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droTheTitleFormat_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droTitleIncludeRootDrive_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droFolderItemCount_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droDisplayEmptyFolder_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droMenuCounterFolder_visual, disabled_enabled, 2);
    add_drop_down_options(ui_droMenuCounterFile_visual, disabled_enabled, 2);

    overlay_count = load_overlay_set(ui_droOverlayImage_visual);

    char *transparency_string = generate_number_string(0, 255, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOverlayTransparency_visual, transparency_string);
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

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

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

    list_nav_move(direct_to_previous(ui_objects, UI_COUNT, &nav_moved), +1);

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
