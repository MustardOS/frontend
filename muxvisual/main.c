#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
char *current_wall = "";
int current_item_index = 0;
int ui_count = 0;

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int battery_total, battery_current;
int network_total, network_current;
int bluetooth_total, bluetooth_current;
int mux_clock_total, mux_clock_current;
int boxart_total, boxart_current;
int boxartalign_total, boxartalign_current;
int name_total, name_current;
int dash_total, dash_current;
int friendlyfolder_total, friendlyfolder_current;
int thetitleformat_total, thetitleformat_current;
int titleincluderootdrive_total, titleincluderootdrive_current;
int folderitemcount_total, folderitemcount_current;
int menu_counter_folder_total, menu_counter_folder_current;
int menu_counter_file_total, menu_counter_file_current;
int background_animation_total, background_animation_current;

#define UI_COUNT 15
lv_obj_t *ui_objects[UI_COUNT];

typedef struct {
    int *total;
    int *current;
} Visuals;

Visuals battery, network, bluetooth, mux_clock, boxart, boxartalign, name, dash, friendlyfolder,
        thetitleformat, titleincluderootdrive, folderitemcount, counterfolder, counterfile, backgroundanimation;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

struct help_msg {
    lv_obj_t *element;
    char *message;
};

void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblBattery,               "Toggle the visibility of the battery glyph"},
            {ui_lblNetwork,               "Toggle the visibility of the network glyph"},
            {ui_lblBluetooth,             "Toggle the visibility of the bluetooth glyph"},
            {ui_lblClock,                 "Toggle the visibility of the clock"},
            {ui_lblBoxArt,                "Change the display priority of the content images"},
            {ui_lblBoxArtAlign,           "Change the screen alignment of the content images"},
            {ui_lblName,                  "Remove extra information from content labels - This does NOT rename your "
                                          "files it only changes how it is displayed"},
            {ui_lblDash,                  "Replaces the dash (-) with a colon (:) for content labels"},
            {ui_lblFriendlyFolder,        "Replaces the label of shortened content folders to more "
                                          "appropriately named labels"},
            {ui_lblTheTitleFormat,        "Rearranges the label of content to move the 'The' label to the front - "
                                          "For example, 'Batman and Robin, The' to 'The Batman and Robin'"},
            {ui_lblTitleIncludeRootDrive, "Changes the top title label in Explore Content to show current storage"
                                          " device along with folder name"},
            {ui_lblFolderItemCount,       "Toggle the visibility of the item count within folders in Explore Content"},
            {ui_lblMenuCounterFolder,     "Toggle the visibility of currently selected folder along "
                                          "with total in Explore Content"},
            {ui_lblMenuCounterFile,       "Toggle the visibility of currently selected file along "
                                          "with total in Explore Content"},
            {ui_lblBackgroundAnimation,   "Toggle the background animation of the current selected theme"},
    };

    char *message = TG("No Help Information Found");
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = TG("No Help Information Found");

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), TS(message));
}

void init_pointers(Visuals *visuals, int *total, int *current) {
    visuals->total = total;
    visuals->current = current;
}

static void dropdown_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        char buf[MAX_BUFFER_SIZE];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
    }
}

void elements_events_init() {
    lv_obj_t *dropdowns[] = {
            ui_droBattery,
            ui_droNetwork,
            ui_droBluetooth,
            ui_droClock,
            ui_droBoxArt,
            ui_droBoxArtAlign,
            ui_droName,
            ui_droDash,
            ui_droFriendlyFolder,
            ui_droTheTitleFormat,
            ui_droTitleIncludeRootDrive,
            ui_droFolderItemCount,
            ui_droMenuCounterFolder,
            ui_droMenuCounterFile,
            ui_droBackgroundAnimation,
    };

    for (unsigned int i = 0; i < sizeof(dropdowns) / sizeof(dropdowns[0]); i++) {
        lv_obj_add_event_cb(dropdowns[i], dropdown_event_handler, LV_EVENT_ALL, NULL);
    }

    init_pointers(&battery, &battery_total, &battery_current);
    init_pointers(&network, &network_total, &network_current);
    init_pointers(&bluetooth, &bluetooth_total, &bluetooth_current);
    init_pointers(&mux_clock, &mux_clock_total, &mux_clock_current);
    init_pointers(&boxart, &boxart_total, &boxart_current);
    init_pointers(&boxartalign, &boxartalign_total, &boxartalign_current);
    init_pointers(&name, &name_total, &name_current);
    init_pointers(&dash, &dash_total, &dash_current);
    init_pointers(&friendlyfolder, &friendlyfolder_total, &friendlyfolder_current);
    init_pointers(&thetitleformat, &thetitleformat_total, &thetitleformat_current);
    init_pointers(&titleincluderootdrive, &titleincluderootdrive_total, &titleincluderootdrive_current);
    init_pointers(&folderitemcount, &folderitemcount_total, &folderitemcount_current);
    init_pointers(&counterfolder, &menu_counter_folder_total, &menu_counter_folder_current);
    init_pointers(&counterfile, &menu_counter_file_total, &menu_counter_file_current);
    init_pointers(&backgroundanimation, &background_animation_total, &background_animation_current);
}

void init_dropdown_settings() {
    Visuals settings[] = {
            {battery.total,               battery.current},
            {network.total,               network.current},
            {bluetooth.total,             bluetooth.current},
            {mux_clock.total,             mux_clock.current},
            {boxart.total,                boxart.current},
            {boxartalign.total,           boxartalign.current},
            {name.total,                  name.current},
            {dash.total,                  dash.current},
            {friendlyfolder.total,        friendlyfolder.current},
            {thetitleformat.total,        thetitleformat.current},
            {titleincluderootdrive.total, titleincluderootdrive.current},
            {folderitemcount.total,       folderitemcount.current},
            {counterfolder.total,         counterfolder.current},
            {counterfile.total,           counterfile.current},
            {backgroundanimation.total,   backgroundanimation.current}
    };

    lv_obj_t *dropdowns[] = {
            ui_droBattery,
            ui_droNetwork,
            ui_droBluetooth,
            ui_droClock,
            ui_droBoxArt,
            ui_droBoxArtAlign,
            ui_droName,
            ui_droDash,
            ui_droFriendlyFolder,
            ui_droTheTitleFormat,
            ui_droTitleIncludeRootDrive,
            ui_droFolderItemCount,
            ui_droMenuCounterFolder,
            ui_droMenuCounterFile,
            ui_droBackgroundAnimation
    };

    for (unsigned int i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        *(settings[i].total) = lv_dropdown_get_option_cnt(dropdowns[i]);
        *(settings[i].current) = lv_dropdown_get_selected(dropdowns[i]);
    }
}

void restore_visual_options() {
    lv_dropdown_set_selected(ui_droBattery, config.VISUAL.BATTERY);
    lv_dropdown_set_selected(ui_droNetwork, config.VISUAL.NETWORK);
    lv_dropdown_set_selected(ui_droBluetooth, config.VISUAL.BLUETOOTH);
    lv_dropdown_set_selected(ui_droClock, config.VISUAL.CLOCK);
    lv_dropdown_set_selected(ui_droBoxArt, config.VISUAL.BOX_ART);
    lv_dropdown_set_selected(ui_droBoxArtAlign, config.VISUAL.BOX_ART_ALIGN - 1);
    lv_dropdown_set_selected(ui_droName, config.VISUAL.NAME);
    lv_dropdown_set_selected(ui_droDash, config.VISUAL.DASH);
    lv_dropdown_set_selected(ui_droFriendlyFolder, config.VISUAL.FRIENDLYFOLDER);
    lv_dropdown_set_selected(ui_droTheTitleFormat, config.VISUAL.THETITLEFORMAT);
    lv_dropdown_set_selected(ui_droTitleIncludeRootDrive, config.VISUAL.TITLEINCLUDEROOTDRIVE);
    lv_dropdown_set_selected(ui_droFolderItemCount, config.VISUAL.FOLDERITEMCOUNT);
    lv_dropdown_set_selected(ui_droMenuCounterFolder, config.VISUAL.COUNTERFOLDER);
    lv_dropdown_set_selected(ui_droMenuCounterFile, config.VISUAL.COUNTERFILE);
    lv_dropdown_set_selected(ui_droBackgroundAnimation, config.VISUAL.BACKGROUNDANIMATION);
}

void save_visual_options() {
    int idx_battery = lv_dropdown_get_selected(ui_droBattery);
    int idx_network = lv_dropdown_get_selected(ui_droNetwork);
    int idx_bluetooth = lv_dropdown_get_selected(ui_droBluetooth);
    int idx_clock = lv_dropdown_get_selected(ui_droClock);
    int idx_boxart = lv_dropdown_get_selected(ui_droBoxArt);
    int idx_boxartalign = lv_dropdown_get_selected(ui_droBoxArtAlign) + 1;
    int idx_name = lv_dropdown_get_selected(ui_droName);
    int idx_dash = lv_dropdown_get_selected(ui_droDash);
    int idx_friendlyfolder = lv_dropdown_get_selected(ui_droFriendlyFolder);
    int idx_thetitleformat = lv_dropdown_get_selected(ui_droTheTitleFormat);
    int idx_titleincluderootdrive = lv_dropdown_get_selected(ui_droTitleIncludeRootDrive);
    int idx_folderitemcount = lv_dropdown_get_selected(ui_droFolderItemCount);
    int idx_counterfolder = lv_dropdown_get_selected(ui_droMenuCounterFolder);
    int idx_counterfile = lv_dropdown_get_selected(ui_droMenuCounterFile);
    int idx_backgroundanimation = lv_dropdown_get_selected(ui_droBackgroundAnimation);

    write_text_to_file("/run/muos/global/visual/battery", "w", INT, idx_battery);
    write_text_to_file("/run/muos/global/visual/network", "w", INT, idx_network);
    write_text_to_file("/run/muos/global/visual/bluetooth", "w", INT, idx_bluetooth);
    write_text_to_file("/run/muos/global/visual/clock", "w", INT, idx_clock);
    write_text_to_file("/run/muos/global/visual/boxart", "w", INT, idx_boxart);
    write_text_to_file("/run/muos/global/visual/boxartalign", "w", INT, idx_boxartalign);
    write_text_to_file("/run/muos/global/visual/name", "w", INT, idx_name);
    write_text_to_file("/run/muos/global/visual/dash", "w", INT, idx_dash);
    write_text_to_file("/run/muos/global/visual/friendlyfolder", "w", INT, idx_friendlyfolder);
    write_text_to_file("/run/muos/global/visual/thetitleformat", "w", INT, idx_thetitleformat);
    write_text_to_file("/run/muos/global/visual/titleincluderootdrive", "w", INT, idx_titleincluderootdrive);
    write_text_to_file("/run/muos/global/visual/folderitemcount", "w", INT, idx_folderitemcount);
    write_text_to_file("/run/muos/global/visual/counterfolder", "w", INT, idx_counterfolder);
    write_text_to_file("/run/muos/global/visual/counterfile", "w", INT, idx_counterfile);
    write_text_to_file("/run/muos/global/visual/backgroundanimation", "w", INT, idx_backgroundanimation);
}

void init_navigation_groups() {
    lv_obj_t *ui_objects_panel[] = {
            ui_pnlBattery,
            ui_pnlNetwork,
            ui_pnlBluetooth,
            ui_pnlClock,
            ui_pnlBoxArt,
            ui_pnlBoxArtAlign,
            ui_pnlName,
            ui_pnlDash,
            ui_pnlFriendlyFolder,
            ui_pnlTheTitleFormat,
            ui_pnlTitleIncludeRootDrive,
            ui_pnlFolderItemCount,
            ui_pnlMenuCounterFolder,
            ui_pnlMenuCounterFile,
            ui_pnlBackgroundAnimation
    };

    ui_objects[0] = ui_lblBattery;
    ui_objects[1] = ui_lblNetwork;
    ui_objects[2] = ui_lblBluetooth;
    ui_objects[3] = ui_lblClock;
    ui_objects[4] = ui_lblBoxArt;
    ui_objects[5] = ui_lblBoxArtAlign;
    ui_objects[6] = ui_lblName;
    ui_objects[7] = ui_lblDash;
    ui_objects[8] = ui_lblFriendlyFolder;
    ui_objects[9] = ui_lblTheTitleFormat;
    ui_objects[10] = ui_lblTitleIncludeRootDrive;
    ui_objects[11] = ui_lblFolderItemCount;
    ui_objects[12] = ui_lblMenuCounterFolder;
    ui_objects[13] = ui_lblMenuCounterFile;
    ui_objects[14] = ui_lblBackgroundAnimation;

    lv_obj_t *ui_objects_value[] = {
            ui_droBattery,
            ui_droNetwork,
            ui_droBluetooth,
            ui_droClock,
            ui_droBoxArt,
            ui_droBoxArtAlign,
            ui_droName,
            ui_droDash,
            ui_droFriendlyFolder,
            ui_droTheTitleFormat,
            ui_droTitleIncludeRootDrive,
            ui_droFolderItemCount,
            ui_droMenuCounterFolder,
            ui_droMenuCounterFile,
            ui_droBackgroundAnimation
    };

    lv_obj_t *ui_objects_glyph[] = {
            ui_icoBattery,
            ui_icoNetwork,
            ui_icoBluetooth,
            ui_icoClock,
            ui_icoBoxArt,
            ui_icoBoxArtAlign,
            ui_icoName,
            ui_icoDash,
            ui_icoFriendlyFolder,
            ui_icoTheTitleFormat,
            ui_icoTitleIncludeRootDrive,
            ui_icoFolderItemCount,
            ui_icoMenuCounterFolder,
            ui_icoMenuCounterFile,
            ui_icoBackgroundAnimation
    };

    apply_theme_list_panel(&theme, &device, ui_pnlBattery);
    apply_theme_list_panel(&theme, &device, ui_pnlNetwork);
    apply_theme_list_panel(&theme, &device, ui_pnlBluetooth);
    apply_theme_list_panel(&theme, &device, ui_pnlClock);
    apply_theme_list_panel(&theme, &device, ui_pnlBoxArt);
    apply_theme_list_panel(&theme, &device, ui_pnlBoxArtAlign);
    apply_theme_list_panel(&theme, &device, ui_pnlName);
    apply_theme_list_panel(&theme, &device, ui_pnlDash);
    apply_theme_list_panel(&theme, &device, ui_pnlFriendlyFolder);
    apply_theme_list_panel(&theme, &device, ui_pnlTheTitleFormat);
    apply_theme_list_panel(&theme, &device, ui_pnlTitleIncludeRootDrive);
    apply_theme_list_panel(&theme, &device, ui_pnlFolderItemCount);
    apply_theme_list_panel(&theme, &device, ui_pnlMenuCounterFolder);
    apply_theme_list_panel(&theme, &device, ui_pnlMenuCounterFile);
    apply_theme_list_panel(&theme, &device, ui_pnlBackgroundAnimation);

    apply_theme_list_item(&theme, ui_lblBattery, TS("Battery"), false, true);
    apply_theme_list_item(&theme, ui_lblNetwork, TS("Network"), false, true);
    apply_theme_list_item(&theme, ui_lblBluetooth, TS("Bluetooth"), false, true);
    apply_theme_list_item(&theme, ui_lblClock, TS("Clock"), false, true);
    apply_theme_list_item(&theme, ui_lblBoxArt, TS("Content Box Art"), false, true);
    apply_theme_list_item(&theme, ui_lblBoxArtAlign, TS("Content Box Art Alignment"), false, true);
    apply_theme_list_item(&theme, ui_lblName, TS("Content Name Scheme"), false, true);
    apply_theme_list_item(&theme, ui_lblDash, TS("Content Dash Replacement"), false, true);
    apply_theme_list_item(&theme, ui_lblFriendlyFolder, TS("Friendly Folder Names"), false, true);
    apply_theme_list_item(&theme, ui_lblTheTitleFormat, TS("Display Title Reformatting"), false, true);
    apply_theme_list_item(&theme, ui_lblTitleIncludeRootDrive, TS("Title Include Root Drive"), false, true);
    apply_theme_list_item(&theme, ui_lblFolderItemCount, TS("Folder Item Count"), false, true);
    apply_theme_list_item(&theme, ui_lblMenuCounterFolder, TS("Menu Counter Folder"), false, true);
    apply_theme_list_item(&theme, ui_lblMenuCounterFile, TS("Menu Counter File"), false, true);
    apply_theme_list_item(&theme, ui_lblBackgroundAnimation, TS("Background Animation"), false, true);

    apply_theme_list_glyph(&theme, ui_icoBattery, mux_module, "battery");
    apply_theme_list_glyph(&theme, ui_icoNetwork, mux_module, "network");
    apply_theme_list_glyph(&theme, ui_icoBluetooth, mux_module, "bluetooth");
    apply_theme_list_glyph(&theme, ui_icoClock, mux_module, "clock");
    apply_theme_list_glyph(&theme, ui_icoBoxArt, mux_module, "boxart");
    apply_theme_list_glyph(&theme, ui_icoBoxArtAlign, mux_module, "boxartalign");
    apply_theme_list_glyph(&theme, ui_icoName, mux_module, "name");
    apply_theme_list_glyph(&theme, ui_icoDash, mux_module, "dash");
    apply_theme_list_glyph(&theme, ui_icoFriendlyFolder, mux_module, "friendlyfolder");
    apply_theme_list_glyph(&theme, ui_icoTheTitleFormat, mux_module, "thetitleformat");
    apply_theme_list_glyph(&theme, ui_icoTitleIncludeRootDrive, mux_module, "titleincluderootdrive");
    apply_theme_list_glyph(&theme, ui_icoFolderItemCount, mux_module, "folderitemcount");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFolder, mux_module, "counterfolder");
    apply_theme_list_glyph(&theme, ui_icoMenuCounterFile, mux_module, "counterfile");
    apply_theme_list_glyph(&theme, ui_icoBackgroundAnimation, mux_module, "backgroundanimation");

    apply_theme_list_drop_down(&theme, ui_droBattery, NULL);
    apply_theme_list_drop_down(&theme, ui_droNetwork, NULL);
    apply_theme_list_drop_down(&theme, ui_droBluetooth, NULL);
    apply_theme_list_drop_down(&theme, ui_droClock, NULL);
    apply_theme_list_drop_down(&theme, ui_droBoxArt, NULL);
    apply_theme_list_drop_down(&theme, ui_droBoxArtAlign, NULL);
    apply_theme_list_drop_down(&theme, ui_droName, NULL);
    apply_theme_list_drop_down(&theme, ui_droDash, NULL);
    apply_theme_list_drop_down(&theme, ui_droFriendlyFolder, NULL);
    apply_theme_list_drop_down(&theme, ui_droTheTitleFormat, NULL);
    apply_theme_list_drop_down(&theme, ui_droTitleIncludeRootDrive, NULL);
    apply_theme_list_drop_down(&theme, ui_droFolderItemCount, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFolder, NULL);
    apply_theme_list_drop_down(&theme, ui_droMenuCounterFile, NULL);
    apply_theme_list_drop_down(&theme, ui_droBackgroundAnimation, NULL);

    char *hidden_visible[] = {TG("Hidden"), TG("Visible")};
    char *disabled_enabled[] = {TG("Disabled"), TG("Enabled")};
    add_drop_down_options(ui_droBattery, hidden_visible, 2);
    add_drop_down_options(ui_droNetwork, hidden_visible, 2);
    add_drop_down_options(ui_droBluetooth, hidden_visible, 2);
    add_drop_down_options(ui_droClock, hidden_visible, 2);
    add_drop_down_options(ui_droBoxArt, (char *[]) {
            TS("Behind"), TS("Front"), TS("Fullscreen + Behind"), TS("Fullscreen + Front"), TG("Disabled")}, 5);
    add_drop_down_options(ui_droBoxArtAlign, (char *[]) {
            TS("Top Left"), TS("Top Middle"), TS("Top Right"),
            TS("Bottom Left"), TS("Bottom Middle"), TS("Bottom Right"),
            TS("Middle Left"), TS("Middle Right"), TS("Center")}, 9);
    add_drop_down_options(ui_droName,
                          (char *[]) {TS("Full Name"), TS("Remove [ ]"), TS("Remove ( )"), TS("Remove [ ] and ( )")},
                          4);
    add_drop_down_options(ui_droDash, disabled_enabled, 2);
    add_drop_down_options(ui_droFriendlyFolder, disabled_enabled, 2);
    add_drop_down_options(ui_droTheTitleFormat, disabled_enabled, 2);
    add_drop_down_options(ui_droTitleIncludeRootDrive, disabled_enabled, 2);
    add_drop_down_options(ui_droFolderItemCount, hidden_visible, 2);
    add_drop_down_options(ui_droMenuCounterFolder, hidden_visible, 2);
    add_drop_down_options(ui_droMenuCounterFile, hidden_visible, 2);
    add_drop_down_options(ui_droBackgroundAnimation, disabled_enabled, 2);

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

}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_value, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    play_sound("navigate", nav_sound, 0);
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

void handle_option_prev(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("navigate", nav_sound, 0);
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblBattery) {
        decrease_option_value(ui_droBattery,
                              &battery_current,
                              battery_total);
    } else if (element_focused == ui_lblNetwork) {
        decrease_option_value(ui_droNetwork,
                              &network_current,
                              network_total);
    } else if (element_focused == ui_lblBluetooth) {
        decrease_option_value(ui_droBluetooth,
                              &bluetooth_current,
                              bluetooth_total);
    } else if (element_focused == ui_lblClock) {
        decrease_option_value(ui_droClock,
                              &mux_clock_current,
                              mux_clock_total);
    } else if (element_focused == ui_lblBoxArt) {
        decrease_option_value(ui_droBoxArt,
                              &boxart_current,
                              boxart_total);
    } else if (element_focused == ui_lblBoxArtAlign) {
        decrease_option_value(ui_droBoxArtAlign,
                              &boxartalign_current,
                              boxartalign_total);
    } else if (element_focused == ui_lblName) {
        decrease_option_value(ui_droName,
                              &name_current,
                              name_total);
    } else if (element_focused == ui_lblDash) {
        decrease_option_value(ui_droDash,
                              &dash_current,
                              dash_total);
    } else if (element_focused == ui_lblFriendlyFolder) {
        decrease_option_value(ui_droFriendlyFolder,
                              &friendlyfolder_current,
                              friendlyfolder_total);
    } else if (element_focused == ui_lblTheTitleFormat) {
        decrease_option_value(ui_droTheTitleFormat,
                              &thetitleformat_current,
                              thetitleformat_total);
    } else if (element_focused == ui_lblTitleIncludeRootDrive) {
        decrease_option_value(ui_droTitleIncludeRootDrive,
                              &titleincluderootdrive_current,
                              titleincluderootdrive_total);
    } else if (element_focused == ui_lblFolderItemCount) {
        decrease_option_value(ui_droFolderItemCount,
                              &folderitemcount_current,
                              folderitemcount_total);
    } else if (element_focused == ui_lblMenuCounterFolder) {
        decrease_option_value(ui_droMenuCounterFolder,
                              &menu_counter_folder_current,
                              menu_counter_folder_total);
    } else if (element_focused == ui_lblMenuCounterFile) {
        decrease_option_value(ui_droMenuCounterFile,
                              &menu_counter_file_current,
                              menu_counter_file_total);
    } else if (element_focused == ui_lblBackgroundAnimation) {
        decrease_option_value(ui_droBackgroundAnimation,
                              &background_animation_current,
                              background_animation_total);
    }
}

void handle_option_next(void) {
    if (msgbox_active) {
        return;
    }

    play_sound("navigate", nav_sound, 0);
    struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
    if (element_focused == ui_lblBattery) {
        increase_option_value(ui_droBattery,
                              &battery_current,
                              battery_total);
    } else if (element_focused == ui_lblNetwork) {
        increase_option_value(ui_droNetwork,
                              &network_current,
                              network_total);
    } else if (element_focused == ui_lblBluetooth) {
        increase_option_value(ui_droBluetooth,
                              &bluetooth_current,
                              bluetooth_total);
    } else if (element_focused == ui_lblClock) {
        increase_option_value(ui_droClock,
                              &mux_clock_current,
                              mux_clock_total);
    } else if (element_focused == ui_lblBoxArt) {
        increase_option_value(ui_droBoxArt,
                              &boxart_current,
                              boxart_total);
    } else if (element_focused == ui_lblBoxArtAlign) {
        increase_option_value(ui_droBoxArtAlign,
                              &boxartalign_current,
                              boxartalign_total);
    } else if (element_focused == ui_lblName) {
        increase_option_value(ui_droName,
                              &name_current,
                              name_total);
    } else if (element_focused == ui_lblDash) {
        increase_option_value(ui_droDash,
                              &dash_current,
                              dash_total);
    } else if (element_focused == ui_lblTheTitleFormat) {
        increase_option_value(ui_droTheTitleFormat,
                              &thetitleformat_current,
                              thetitleformat_total);
    } else if (element_focused == ui_lblTitleIncludeRootDrive) {
        increase_option_value(ui_droTitleIncludeRootDrive,
                              &titleincluderootdrive_current,
                              titleincluderootdrive_total);
    } else if (element_focused == ui_lblFriendlyFolder) {
        increase_option_value(ui_droFriendlyFolder,
                              &folderitemcount_current,
                              friendlyfolder_total);
    } else if (element_focused == ui_lblFolderItemCount) {
        increase_option_value(ui_droFolderItemCount,
                              &folderitemcount_current,
                              folderitemcount_total);
    } else if (element_focused == ui_lblMenuCounterFolder) {
        increase_option_value(ui_droMenuCounterFolder,
                              &menu_counter_folder_current,
                              menu_counter_folder_total);
    } else if (element_focused == ui_lblMenuCounterFile) {
        increase_option_value(ui_droMenuCounterFile,
                              &menu_counter_file_current,
                              menu_counter_file_total);
    } else if (element_focused == ui_lblBackgroundAnimation) {
        increase_option_value(ui_droBackgroundAnimation,
                              &background_animation_current,
                              background_animation_total);
    }
}

void handle_back(void) {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 1);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound("back", nav_sound, 1);
    input_disable = 1;

    osd_message = TG("Saving Changes");
    lv_label_set_text(ui_lblMessage, osd_message);
    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

    save_visual_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "interface");
    mux_input_stop();
}

void handle_help(void) {
    if (msgbox_active) {
        return;
    }

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 1);
        show_help(lv_group_get_focused(ui_group));
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgressBrightness);
    lv_obj_move_foreground(ui_pnlProgressVolume);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, TG("Save"));

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblBattery, "battery");
    lv_obj_set_user_data(ui_lblNetwork, "network");
    lv_obj_set_user_data(ui_lblBluetooth, "bluetooth");
    lv_obj_set_user_data(ui_lblClock, "clock");
    lv_obj_set_user_data(ui_lblBoxArt, "boxart");
    lv_obj_set_user_data(ui_lblBoxArtAlign, "boxartalign");
    lv_obj_set_user_data(ui_lblName, "name");
    lv_obj_set_user_data(ui_lblDash, "dash");
    lv_obj_set_user_data(ui_lblFriendlyFolder, "friendlyfolder");
    lv_obj_set_user_data(ui_lblTheTitleFormat, "thetitleformat");
    lv_obj_set_user_data(ui_lblTitleIncludeRootDrive, "titleincluderootdrive");
    lv_obj_set_user_data(ui_lblFolderItemCount, "folderitemcount");
    lv_obj_set_user_data(ui_lblMenuCounterFolder, "counterfolder");
    lv_obj_set_user_data(ui_lblMenuCounterFile, "counterfile");
    lv_obj_set_user_data(ui_lblBackgroundAnimation, "backgroundanimation");

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlNetwork, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlNetwork, LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
    }

    if (!device.DEVICE.HAS_BLUETOOTH || true) { //TODO: remove true when bluetooth is implemented
        lv_obj_add_flag(ui_pnlBluetooth, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_pnlBluetooth, LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t *overlay_img = lv_img_create(ui_screen);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND));

            if (strcasecmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    if (theme.MISC.RANDOM_BACKGROUND) {
                        load_image_random(ui_imgWall, new_wall);
                    } else {
                        switch (theme.MISC.ANIMATED_BACKGROUND) {
                            case 1:
                                lv_gif_set_src(lv_gif_create(ui_pnlWall), new_wall);
                                break;
                            case 2:
                                load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, new_wall);
                                break;
                            default:
                                lv_img_set_src(ui_imgWall, new_wall);
                                break;
                        }
                    }
                } else {
                    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_screen, ui_group));

            if (strlen(static_image) > 0) {
                printf("LOADING STATIC IMAGE: %s\n", static_image);

                switch (theme.MISC.STATIC_ALIGNMENT) {
                    case 0: // Bottom + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 1: // Middle + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 2: // Top + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 3: // Fullscreen + Behind
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            }
        }
        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    load_device(&device);


    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_module);

    ui_common_screen_init(&theme, &device, TS("INTERFACE OPTIONS"));
    ui_init(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.RANDOM_BACKGROUND) {
            load_image_random(ui_imgWall, current_wall);
        } else {
            switch (theme.MISC.ANIMATED_BACKGROUND) {
                case 1:
                    lv_gif_set_src(lv_gif_create(ui_pnlWall), current_wall);
                    break;
                case 2:
                    load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, current_wall);
                    break;
                default:
                    lv_img_set_src(ui_imgWall, current_wall);
                    break;
            }
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_module, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_module, FONT_FOOTER_FOLDER, ui_pnlFooter);

    nav_sound = init_nav_sound(mux_module);
    init_navigation_groups();
    elements_events_init();

    restore_visual_options();
    init_dropdown_settings();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    init_elements();
    refresh_screen();
    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = 16 /* ~60 FPS */,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_option_next,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    // List navigation:
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_bright,
                            .hold_handler = ui_common_handle_bright,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_UP),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_VOL_DOWN),
                            .press_handler = ui_common_handle_vol,
                            .hold_handler = ui_common_handle_vol,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
